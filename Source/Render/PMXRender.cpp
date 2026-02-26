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
    // マテリアルごとの描画単位を作成
    UINT start = 0;
    for (size_t i = 0; i < m_pmxFileData.materials.size(); ++i)
    {
        Subset s{};
        s.startIndex = start;
        s.indexCount = m_pmxFileData.materials[i].numFaceVertices;
        s.materialIndex = (UINT)i;
        int texIndex = m_pmxFileData.materials[i].textureIndex;
        if (texIndex >= 0)
        {
            s.textureIndices.push_back(texIndex);
        }
        s.visible = true;

        start += s.indexCount;

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
    if (!file) return;

    size_t subsetCount = 0;
    file.read(reinterpret_cast<char*>(&subsetCount), sizeof(size_t));

    if (subsetCount != m_subsets.size())
        return;

    for (size_t i = 0; i < subsetCount; ++i)
    {
        auto& subset = m_subsets[i];

        //! visible読込
        file.read(reinterpret_cast<char*>(&subset.visible), sizeof(bool));

        //! テクスチャ数読込
        size_t texCount = 0;
        file.read(reinterpret_cast<char*>(&texCount), sizeof(size_t));

        subset.textureIndices.clear();

        for (size_t t = 0; t < texCount; ++t)
        {
            size_t len = 0;
            file.read(reinterpret_cast<char*>(&len), sizeof(size_t));

            std::wstring path;
            path.resize(len);

            if (len > 0)
            {
                file.read(reinterpret_cast<char*>(path.data()), len * sizeof(wchar_t));

                auto tex = TextureManager::Instance().load(path);

                if (tex)
                {
                    int newIndex = (int)m_textures.size();
                    m_textures.push_back(tex);
                    m_texturePaths.push_back(path);

                    subset.textureIndices.push_back(newIndex);
                }
                else
                {
                    subset.textureIndices.push_back(-1);
                }
            }
            else
            {
                subset.textureIndices.push_back(-1);
            }
        }
    }
}

void PMXRender::saveSetting()
{
    std::ofstream file(m_settingPath, std::ios::binary);
    if (!file) return;

    size_t subsetCount = m_subsets.size();
    file.write(reinterpret_cast<const char*>(&subsetCount), sizeof(size_t));

    for (const auto& subset : m_subsets)
    {
        //! visible保存
        file.write(reinterpret_cast<const char*>(&subset.visible), sizeof(bool));

        //! テクスチャ数保存
        size_t texCount = subset.textureIndices.size();
        file.write(reinterpret_cast<const char*>(&texCount), sizeof(size_t));

        for (size_t t = 0; t < texCount; ++t)
        {
            int texIndex = subset.textureIndices[t];

            std::wstring path;
            if (texIndex >= 0 && texIndex < m_texturePaths.size())
                path = m_texturePaths[texIndex];

            size_t len = path.size();
            file.write(reinterpret_cast<const char*>(&len), sizeof(size_t));

            if (len > 0)
            {
                file.write(reinterpret_cast<const char*>(path.c_str()), len * sizeof(wchar_t));
            }
        }
    }
}

void PMXRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! DescriptorHeap
    DescriptorHeapManager::Instance().setDiscriptorHeap();

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

    // モデル行列を更新（Transform が紐付いていればそれを使用）
    Matrix world = Matrix::Identity;
    if (m_transform)
    {
        world = m_transform->getWorldMatrix();
    }
    // シェーダー側の行列扱い（カメラ側と合わせるため転置して渡す）
    Model modelData{};
    modelData.world = world.Transpose();
    m_modelCB->update(modelData);

    //! DescriptorTable(モデル行列)
    cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

    for (const auto& subset : m_subsets)
    {
        //! DescriptorTable(テクスチャ)
        for (int i = 0; i < subset.textureIndices.size(); ++i)
        {
            int texIndex = subset.textureIndices[i];
            if (texIndex >= 0 && texIndex < m_textures.size())
            {
                cmd->SetGraphicsRootDescriptorTable(3, m_textures[texIndex]->getGPUHandle());
            }
        }

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
    if (ImGui::Begin("PMX Editor"))
    {
        for (size_t i = 0; i < m_subsets.size(); ++i)
        {
            auto& subset = m_subsets[i];
            const auto& pmxMat = m_pmxFileData.materials[i];

            ImGui::PushID((int)i);

            ImGui::Separator();

            //! マテリアル名
            std::string matName(pmxMat.name.begin(), pmxMat.name.end());
            ImGui::Text("Name : %s", matName.c_str());

            if (ImGui::Checkbox("Visible", &subset.visible))
            {
                saveSetting();
            }

            ImGui::Spacing();

            //! テクスチャ一覧
            for (size_t t = 0; t < subset.textureIndices.size(); ++t)
            {
                ImGui::PushID((int)t);
                int texIndex = subset.textureIndices[t];
                ImGui::Text("Texture %d", (int)t);

                //! プレビュー
                if (texIndex >= 0 && texIndex < m_textures.size())
                {
                    ImTextureID texID = (ImTextureID)m_textures[texIndex]->getGPUHandle().ptr;
                    ImGui::Image(texID, ImVec2(80, 80));
                }
                else
                {
                    ImGui::Text("No Texture");
                }

                //! Change
                if (ImGui::Button("Change"))
                {
                    std::vector<std::wstring> paths;

                    if (Dialog::openFile(
                        paths,
                        L"テクスチャを選択",
                        L"Data/Model",
                        false) == DialogResult::OK)
                    {
                        auto newTex =
                            TextureManager::Instance().load(paths[0]);

                        if (newTex)
                        {
                            int newIndex = (int)m_textures.size();

                            m_textures.push_back(newTex);
                            m_texturePaths.push_back(paths[0]);

                            subset.textureIndices[t] = newIndex;

                            saveSetting();
                        }
                    }
                }

                ImGui::SameLine();

                //! Remove
                if (ImGui::Button("Remove"))
                {
                    subset.textureIndices.erase(subset.textureIndices.begin() + t);

                    saveSetting();

                    ImGui::PopID();
                    break;
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            //! Add Texture
            if (ImGui::Button("Add Texture"))
            {
                subset.textureIndices.push_back(-1);
                saveSetting();
            }

            ImGui::PopID();
        }
    }
    ImGui::End();
}