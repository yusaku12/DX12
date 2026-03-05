#include "pch.h"
#include "FBXRender.h"
#include "Component\TransformComponent.h"

FBXRender::FBXRender(const std::string& filePath)
{
    m_settingPath = filePath + ".texture";

    //! FBXファイルの読み込み
    FBXLoad loader;
    if (!loader.load(filePath))
    {
        LOG_ASSERT_NO_JUDGE("failure file path");
        return;
    }
    m_model = loader.takeModel();

    //! モデルCBVの作成
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>();
    m_modelCB->update(ModelCB{});

    //! テクスチャの読み込み
    createTextures();

    //! メッシュ描画データの作成
    createMeshData();

    //! マテリアルCBVの作成
    createMaterialCBV();

    //! PSOの作成
    createPSO();

    //! 設定の読み込み
    loadSetting();
}

void FBXRender::createMeshData()
{
    for (const auto& srcMesh : m_model.meshes)
    {
        MeshData meshData;

        //! 頂点データ変換（ベイク済みなのでそのまま使用）
        std::vector<Vertex> vertices;
        vertices.reserve(srcMesh.vertices.size());

        for (const auto& v : srcMesh.vertices)
        {
            Vertex gv{};
            gv.position = v.position;
            gv.normal = v.normal;
            gv.tangent = v.tangent;
            gv.uv = v.uv;
            vertices.push_back(gv);
        }

        //! 頂点バッファの作成
        meshData.vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(vertices);

        //! インデックスバッファの作成
        meshData.indexBuffer = std::make_unique<IndexBuffer<uint32_t>>(srcMesh.indices);

        //! サブセットの作成
        for (const auto& sub : srcMesh.subMeshes)
        {
            Subset s{};
            s.startIndex = sub.startIndex;
            s.indexCount = sub.indexCount;
            s.materialIndex = sub.materialIndex;
            s.textureIndices.fill(-1);

            //! マテリアルに紐づくテクスチャを検索
            if (sub.materialIndex < m_model.materials.size())
            {
                const auto& mat = m_model.materials[sub.materialIndex];

                //! Diffuse テクスチャ
                if (!mat.texturePath.empty())
                {
                    std::wstring wpath(mat.texturePath.begin(), mat.texturePath.end());

                    for (int i = 0; i < (int)m_texturePaths.size(); ++i)
                    {
                        if (m_texturePaths[i] == wpath)
                        {
                            s.textureIndices[static_cast<int>(TextureType::Diffuse)] = i;
                            break;
                        }
                    }
                }

                //! Normal テクスチャ
                if (!mat.normalMapPath.empty())
                {
                    std::wstring wpath(mat.normalMapPath.begin(), mat.normalMapPath.end());

                    for (int i = 0; i < (int)m_texturePaths.size(); ++i)
                    {
                        if (m_texturePaths[i] == wpath)
                        {
                            s.textureIndices[static_cast<int>(TextureType::Normal)] = i;
                            break;
                        }
                    }
                }
            }

            s.descriptorBase = DescriptorHeapManager::Instance().allocateRange(static_cast<int>(TextureType::Max));
            rebuildSubsetDescriptors(s);

            meshData.subsets.push_back(s);
        }

        m_meshes.push_back(std::move(meshData));
    }
}

void FBXRender::createMaterialCBV()
{
    UINT materialCount = (UINT)m_model.materials.size();

    if (materialCount == 0)
        materialCount = 1;

    //! マテリアルCBVの作成
    m_materialCB = std::make_unique<ConstantBuffer<Material>>(materialCount);

    for (UINT i = 0; i < (UINT)m_model.materials.size(); ++i)
    {
        const auto& m = m_model.materials[i];
        Material material{};
        material.diffuse = m.diffuseColor;
        material.specular = m.specularColor;
        material.ambient = m.ambientColor;
        m_materialCB->update(material, i);
    }
}

void FBXRender::createTextures()
{
    for (const auto& mat : m_model.materials)
    {
        //! Diffuse テクスチャ
        if (!mat.texturePath.empty())
        {
            std::wstring wpath(mat.texturePath.begin(), mat.texturePath.end());

            //! 既に読み込み済みか確認
            bool found = false;
            for (const auto& existing : m_texturePaths)
            {
                if (existing == wpath)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                auto texture = TextureManager::Instance().load(wpath);
                m_textures.push_back(texture);
                m_texturePaths.push_back(wpath);
            }
        }

        //! Normal テクスチャ
        if (!mat.normalMapPath.empty())
        {
            std::wstring wpath(mat.normalMapPath.begin(), mat.normalMapPath.end());

            //! 既に読み込み済みか確認
            bool found = false;
            for (const auto& existing : m_texturePaths)
            {
                if (existing == wpath)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                auto texture = TextureManager::Instance().load(wpath);
                m_textures.push_back(texture);
                m_texturePaths.push_back(wpath);
            }
        }
    }
}

void FBXRender::createPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::PMXStandard;
    psoData.vsShaderId = ShaderID::FBXVS;
    psoData.psShaderId = ShaderID::FBXPS;
    psoData.rasterizerState = RasterizerState::CULL_COUNTER_CLOCKWISE;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_psoKey = PSOCreator::Instance().registerPSO(psoData);
}

void FBXRender::rebuildSubsetDescriptors(Subset& subset)
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
            srvIndices.push_back(m_textures[0]->getSRVIndex());
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subset.descriptorBase, srvIndices);
}

void FBXRender::loadSetting()
{
    std::ifstream file(m_settingPath, std::ios::binary);
    if (!file)
        return;

    //! 全サブセットの合計数を読み込む
    size_t totalSubsetCount = 0;
    file.read(reinterpret_cast<char*>(&totalSubsetCount), sizeof(size_t));

    //! 現在の合計サブセット数を算出
    size_t currentTotal = 0;
    for (const auto& mesh : m_meshes)
        currentTotal += mesh.subsets.size();

    if (!file || totalSubsetCount != currentTotal)
        return;

    for (auto& mesh : m_meshes)
    {
        for (auto& subset : mesh.subsets)
        {
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
}

void FBXRender::saveSetting()
{
    std::ofstream file(m_settingPath, std::ios::binary | std::ios::trunc);

    if (!file)
        return;

    //! 全サブセットの合計数を書き込む
    size_t totalSubsetCount = 0;
    for (const auto& mesh : m_meshes)
        totalSubsetCount += mesh.subsets.size();

    file.write(reinterpret_cast<const char*>(&totalSubsetCount), sizeof(size_t));

    for (const auto& mesh : m_meshes)
    {
        for (const auto& subset : mesh.subsets)
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
}

void FBXRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    render(cmd);
}

void FBXRender::render(ID3D12GraphicsCommandList* cmd)
{
    //! DescriptorHeap
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    //! RootSignature
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));

    //! PSO
    PSOCreator::Instance().setPSO(m_psoKey, cmd);

    //! IA
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //! CBV(カメラ)
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    //! モデル行列を更新（Transform が紐付いていればそれを使用）
    Matrix world = Matrix::Identity;
    if (m_transform)
    {
        world = m_transform->getWorldMatrix();
    }

    //! カメラと同じ規約: DirectXMath行列を Transpose して row_major シェーダーに渡す
    ModelCB modelData{};
    modelData.world = world.Transpose();
    m_modelCB->update(modelData);

    //! DescriptorTable(モデル行列)
    cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

    //! メッシュごとに描画
    for (const auto& mesh : m_meshes)
    {
        //! VB/IB
        mesh.vertexBuffer->bind(cmd);
        mesh.indexBuffer->bind(cmd);

        for (const auto& subset : mesh.subsets)
        {
            //! DescriptorTable(テクスチャ)
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
}

void FBXRender::debugRender()
{
    if (!ImGui::Begin("FBX Material Editor"))
    {
        ImGui::End();
        return;
    }

    static int selectedMesh = -1;
    static int selectedSubset = -1;

    ImGui::Columns(2, nullptr, true);

    //! Subset List
    ImGui::BeginChild("SubsetList");

    int globalIndex = 0;

    for (int mi = 0; mi < (int)m_meshes.size(); ++mi)
    {
        std::string meshLabel = "Mesh " + std::to_string(mi);

        if (!m_model.meshes.empty() && mi < (int)m_model.meshes.size())
            meshLabel = m_model.meshes[mi].name.empty() ? meshLabel : m_model.meshes[mi].name;

        if (ImGui::TreeNode(meshLabel.c_str()))
        {
            for (int si = 0; si < (int)m_meshes[mi].subsets.size(); ++si)
            {
                std::string name = "Subset " + std::to_string(si);

                UINT matIdx = m_meshes[mi].subsets[si].materialIndex;

                if (matIdx < m_model.materials.size() && !m_model.materials[matIdx].name.empty())
                    name = m_model.materials[matIdx].name;

                bool isSelected = (selectedMesh == mi && selectedSubset == si);

                if (ImGui::Selectable(name.c_str(), isSelected))
                {
                    selectedMesh = mi;
                    selectedSubset = si;
                }

                ++globalIndex;
            }

            ImGui::TreePop();
        }
        else
        {
            globalIndex += (int)m_meshes[mi].subsets.size();
        }
    }

    ImGui::EndChild();

    ImGui::NextColumn();

    //! Inspector
    ImGui::BeginChild("Inspector");

    if (selectedMesh >= 0 && selectedMesh < (int)m_meshes.size() &&
        selectedSubset >= 0 && selectedSubset < (int)m_meshes[selectedMesh].subsets.size())
    {
        auto& subset = m_meshes[selectedMesh].subsets[selectedSubset];

        UINT matIdx = subset.materialIndex;
        std::string matName = "Unknown";

        if (matIdx < m_model.materials.size())
            matName = m_model.materials[matIdx].name.empty() ? "Material " + std::to_string(matIdx) : m_model.materials[matIdx].name;

        ImGui::Text("Material : %s", matName.c_str());
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

                //! テクスチャが設定されている場合のみプレビュー表示
                if (texIndex >= 0 && texIndex < (int)m_textures.size())
                {
                    ImTextureID texID = (ImTextureID)m_textures[texIndex]->getGPUHandle().ptr;

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
                }
                else
                {
                    //! テクスチャ未設定 → 設定ボタンを表示
                    if (ImGui::Button(("Set##" + std::to_string(i)).c_str(), ImVec2(80, 80)))
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
                }

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