#include "pch.h"
#include "PMXRender.h"
#include "Component\TransformComponent.h"

PMXRender::PMXRender(const std::wstring& filePath)
{
    m_settingPath = filePath + L".texture";

    //! PMXファイルの読み込み
    m_pmxLoad = std::make_unique<PmxLoad>(filePath, m_pmxFileData);

    //! モデルCBVの作成
    m_modelCB = std::make_unique<ConstantBuffer<Model>>();
    m_modelCB->update(Model{});

    //! テクスチャの読み込み
    createTextures();

    //! 頂点バッファの作成
    createVertexBuffer();

    //! インデックスバッファの作成
    createIndexBuffer();

    //! マテリアルCBVの作成
    createMaterialCBV();

    //! サブセットの作成
    createSubsets();

    //! PSOの作成
    createPSO();

    //! 設定の読み込み
    loadSetting();
}

void PMXRender::createVertexBuffer()
{
    std::vector<Vertex> vertices;

    for (const auto& v : m_pmxFileData.vertices)
    {
        Vertex gv{};
        gv.position = v.position;
        gv.normal = v.normal;
        gv.uv = v.uv;

        //for (int i = 0; i < 4; ++i)
        //{
        //    gv.boneIndex[i] = v.boneIndices[i];
        //    gv.boneWeight[i] = v.boneWeights[i];
        //}

        vertices.push_back(gv);
    }

    //! 頂点バッファの作成
    m_vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(vertices);
}

void PMXRender::createIndexBuffer()
{
    std::vector<uint32_t> indices;

    for (const auto& f : m_pmxFileData.faces)
    {
        indices.push_back(f.vertices[0]);
        indices.push_back(f.vertices[1]);
        indices.push_back(f.vertices[2]);
    }

    //! インデックスバッファの作成
    m_indexBuffer = std::make_unique<IndexBuffer<unsigned int>>(indices);
}

void PMXRender::createMaterialCBV()
{
    UINT materialCount = (UINT)m_pmxFileData.materials.size();

    //! マテリアルCBVの作成
    m_materialCB = std::make_unique<ConstantBuffer<Material>>(materialCount);

    for (UINT i = 0; i < materialCount; ++i)
    {
        const auto& m = m_pmxFileData.materials[i];
        Material material{};
        material.diffuse = m.diffuse;
        material.specular = m.specular;
        //material.specularPower = m.specularPower;
        material.ambient = m.ambient;
        m_materialCB->update(material, i);
    }
}

void PMXRender::createSubsets()
{
    UINT start = 0;

    for (size_t i = 0; i < m_pmxFileData.materials.size(); ++i)
    {
        Subset s{};
        s.startIndex = start;
        s.indexCount = m_pmxFileData.materials[i].numFaceVertices;
        s.materialIndex = (UINT)i;

        s.textureIndices.fill(-1);

        int texIndex = m_pmxFileData.materials[i].textureIndex;
        if (texIndex >= 0)
            s.textureIndices[0] = texIndex;

        start += s.indexCount;

        s.descriptorBase = DescriptorHeapManager::Instance().allocateRange(static_cast<int>(TextureType::Max));

        rebuildSubsetDescriptors(s);

        m_subsets.push_back(s);
    }
}

void PMXRender::createTextures()
{
    for (auto texName : m_pmxFileData.textures)
    {
        std::wstring fullPath = L"Data/Texture/dummyWhite.jpg";
        auto texture = TextureManager::Instance().load(fullPath);

        m_textures.push_back(texture);
        m_texturePaths.push_back(fullPath);
    }
}

void PMXRender::createPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::PMXStandard;
    psoData.vsShaderId = ShaderID::PMXVS;
    psoData.psShaderId = ShaderID::PMXPS;
    psoData.rasterizerState = RasterizerState::CULL_CLOCKWISE;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_psoCreator = std::make_unique<PSOCreator>(psoData);
}

void PMXRender::loadSetting()
{
    std::ifstream file(m_settingPath, std::ios::binary);
    if (!file)
        return;

    size_t subsetCount = 0;
    file.read(reinterpret_cast<char*>(&subsetCount), sizeof(size_t));

    if (!file || subsetCount != m_subsets.size())
        return;

    for (size_t i = 0; i < subsetCount; ++i)
    {
        auto& subset = m_subsets[i];

        uint8_t visible = 1;
        file.read(reinterpret_cast<char*>(&visible), sizeof(uint8_t));
        subset.visible = (visible != 0);

        //! texture slots (固定数)
        for (UINT t = 0; t < static_cast<int>(TextureType::Max); ++t)
        {
            size_t len = 0;
            file.read(reinterpret_cast<char*>(&len), sizeof(size_t));

            if (!file)
                return;

            if (len == 0)
            {
                subset.textureIndices[t] = -1;
                continue;
            }

            std::wstring path;
            path.resize(len);

            file.read(reinterpret_cast<char*>(path.data()), len * sizeof(wchar_t));

            if (!file)
                return;

            //! 既存テクスチャ検索
            int foundIndex = -1;

            for (size_t k = 0; k < m_texturePaths.size(); ++k)
            {
                if (m_texturePaths[k] == path)
                {
                    foundIndex = (int)k;
                    break;
                }
            }

            //! 無ければロード
            if (foundIndex == -1)
            {
                auto tex = TextureManager::Instance().load(path);

                if (tex)
                {
                    foundIndex = (int)m_textures.size();
                    m_textures.push_back(tex);
                    m_texturePaths.push_back(path);
                }
            }

            subset.textureIndices[t] = foundIndex;
        }

        //! Descriptor再構築
        rebuildSubsetDescriptors(subset);
    }
}

void PMXRender::saveSetting()
{
    std::ofstream file(m_settingPath, std::ios::binary | std::ios::trunc);

    if (!file)
        return;

    size_t subsetCount = m_subsets.size();
    file.write(reinterpret_cast<const char*>(&subsetCount), sizeof(size_t));

    for (const auto& subset : m_subsets)
    {
        uint8_t visible = subset.visible ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&visible), sizeof(uint8_t));

        //! texture slots (固定数)
        for (UINT t = 0; t < static_cast<int>(TextureType::Max); ++t)
        {
            int texIndex = subset.textureIndices[t];

            std::wstring path;

            if (texIndex >= 0 && texIndex < (int)m_texturePaths.size())
            {
                path = m_texturePaths[texIndex];
            }

            size_t len = path.size();
            file.write(reinterpret_cast<const char*>(&len), sizeof(size_t));

            if (len > 0)
            {
                file.write(reinterpret_cast<const char*>(path.data()), len * sizeof(wchar_t));
            }
        }
    }
}

void PMXRender::rebuildSubsetDescriptors(Subset& subset)
{
    if (subset.descriptorBase == UINT_MAX)
        return;

    std::vector<UINT> srvIndices;

    for (UINT i = 0; i < static_cast<int>(TextureType::Max); ++i)
    {
        int texIdx = subset.textureIndices[i];

        if (texIdx >= 0 && texIdx < (int)m_textures.size())
            srvIndices.push_back(m_textures[texIdx]->getSRVIndex());
        else
            srvIndices.push_back(TextureManager::Instance().getWhiteTextureSRVIndex());
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subset.descriptorBase, srvIndices);
}

void PMXRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! DescriptorHeap
    DescriptorHeapManager::Instance().setDescriptorHeap();

    //! RootSignature
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));

    //! PSO
    m_psoCreator->setPSO();

    //! IA
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //! VB/IB
    m_vertexBuffer->bind();
    m_indexBuffer->bind();

    //! CBV(カメラ)
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    //! モデル行列を更新（Transform が紐付いていればそれを使用）
    Matrix world = Matrix::Identity;
    if (m_transform)
    {
        world = m_transform->getWorldMatrix();
    }
    //! シェーダー側の行列扱い（カメラ側と合わせるため転置して渡す）
    Model modelData{};
    modelData.world = world.Transpose();
    m_modelCB->update(modelData);

    //! DescriptorTable(モデル行列)
    cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

    for (const auto& subset : m_subsets)
    {
        //! DescriptorTable(テクスチャ)
        //! ルートパラメータ 3 は MaxTexturesPerMaterial 個分の SRV をまとめたテーブルを期待
        cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));

        //! DescriptorTable(マテリアル)
        cmd->SetGraphicsRootDescriptorTable(2, m_materialCB->getGPUHandle(subset.materialIndex));

        //! Draw
        if (subset.visible)
        {
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}

void PMXRender::debugRender()
{
    if (!ImGui::Begin("PMX Material Editor"))
    {
        ImGui::End();
        return;
    }

    static int selected = -1;

    ImGui::Columns(2, nullptr, true);

    //! Material List
    ImGui::BeginChild("MaterialList");

    for (int i = 0; i < (int)m_subsets.size(); ++i)
    {
        std::string name(m_pmxFileData.materials[i].name.begin(), m_pmxFileData.materials[i].name.end());

        if (ImGui::Selectable(name.c_str(), selected == i))
            selected = i;
    }

    ImGui::EndChild();

    ImGui::NextColumn();

    //! Inspector
    ImGui::BeginChild("Inspector");

    if (selected >= 0)
    {
        auto& subset = m_subsets[selected];

        std::string name(m_pmxFileData.materials[selected].name.begin(), m_pmxFileData.materials[selected].name.end());

        ImGui::Text("Material : %s", name.c_str());
        ImGui::Separator();

        if (ImGui::Checkbox("Visible", &subset.visible))
            saveSetting();

        ImGui::Spacing();
        ImGui::Text("Textures");

        if (ImGui::BeginTable("Textures", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            for (UINT i = 0; i < static_cast<int>(TextureType::Max); ++i)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text(magic_enum::enum_name(TextureType(i)).data(), i);

                ImGui::TableSetColumnIndex(1);

                int texIndex = subset.textureIndices[i];

                ImTextureID texID;

                if (texIndex >= 0 && texIndex < (int)m_textures.size())
                    texID = (ImTextureID)m_textures[texIndex]->getGPUHandle().ptr;
                else
                {
                    auto handle = DescriptorHeapManager::Instance().getGPUHandle(TextureManager::Instance().getWhiteTextureSRVIndex());
                    texID = (ImTextureID)handle.ptr;
                }

                if (ImGui::ImageButton(("TexBtn##" + std::to_string(i)).c_str(), texID, ImVec2(80, 80)))
                {
                    std::vector<std::wstring> paths;

                    if (Dialog::openFile(paths, L"Select Texture", L"Data/Texture", false) == DialogResult::OK)
                    {
                        auto tex = TextureManager::Instance().load(paths[0]);

                        if (tex)
                        {
                            int idx = (int)m_textures.size();
                            m_textures.push_back(tex);
                            m_texturePaths.push_back(paths[0]);

                            subset.textureIndices[i] = idx;

                            rebuildSubsetDescriptors(subset);
                            saveSetting();
                        }
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button(("Clear##" + std::to_string(i)).c_str()))
                {
                    subset.textureIndices[i] = -1;
                    rebuildSubsetDescriptors(subset);
                    saveSetting();
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();

    ImGui::Columns(1);

    ImGui::End();
}