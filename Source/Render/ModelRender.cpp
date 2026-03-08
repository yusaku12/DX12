#include "pch.h"
#include "ModelRender.h"
#include "Component\TransformComponent.h"

ModelRender::ModelRender(const std::string& mdlPath)
{
    m_settingPath = mdlPath + ".texture";

    if (!ModelData::loadFromMdl(mdlPath, m_modelData))
    {
        LOG_ASSERT_NO_JUDGE("Failed to load .mdl: %s", mdlPath.c_str());
        return;
    }

    buildGPUResources();
}

ModelRender::ModelRender(ModelData&& data)
    : m_modelData(std::move(data))
{
    m_settingPath = m_modelData.name + ".texture";
    buildGPUResources();
}

void ModelRender::buildGPUResources()
{
    //! モデル行列CBV
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>();
    m_modelCB->update(ModelCB{});

    //! テクスチャ読み込み
    createTextures();

    //! メッシュ毎に VB / IB / Subsets を構築
    for (const auto& srcMesh : m_modelData.meshes)
    {
        MeshDrawData meshDraw;

        //! 頂点バッファ（ModelVertex はそのまま GPU 送信可能）
        meshDraw.vertexBuffer = std::make_unique<VertexBuffer<ModelVertex>>(srcMesh.vertices);

        //! インデックスバッファ
        meshDraw.indexBuffer = std::make_unique<IndexBuffer<uint32_t>>(srcMesh.indices);

        //! サブセット
        for (const auto& sub : srcMesh.subMeshes)
        {
            Subset s{};
            s.startIndex = sub.startIndex;
            s.indexCount = sub.indexCount;
            s.materialIndex = sub.materialIndex;
            s.textureIndices.fill(-1);

            //! マテリアルに紐づくテクスチャを検索
            if (sub.materialIndex < m_modelData.materials.size())
            {
                const auto& mat = m_modelData.materials[sub.materialIndex];

                auto findTexIndex = [&](const std::string& path) -> int
                    {
                        if (path.empty()) return -1;
                        std::wstring wpath(path.begin(), path.end());
                        for (int i = 0; i < (int)m_texturePaths.size(); ++i)
                        {
                            if (m_texturePaths[i] == wpath) return i;
                        }
                        return -1;
                    };

                s.textureIndices[static_cast<int>(TextureType::Diffuse)] = findTexIndex(mat.diffuseTexPath);
                s.textureIndices[static_cast<int>(TextureType::Normal)] = findTexIndex(mat.normalTexPath);
                s.textureIndices[static_cast<int>(TextureType::Toon)] = findTexIndex(mat.toonTexPath);
            }

            s.descriptorBase = DescriptorHeapManager::Instance().allocateRange(static_cast<int>(TextureType::Max));
            rebuildSubsetDescriptors(s);

            meshDraw.subsets.push_back(s);
        }

        m_meshes.push_back(std::move(meshDraw));
    }

    //! マテリアルCBV
    createMaterialCBV();

    //! PSO
    createPSO();

    //! 設定読み込み
    loadSetting();
}

void ModelRender::createMaterialCBV()
{
    UINT matCount = (UINT)m_modelData.materials.size();
    if (matCount == 0) matCount = 1;

    m_materialCB = std::make_unique<ConstantBuffer<MaterialCB>>(matCount);

    for (UINT i = 0; i < (UINT)m_modelData.materials.size(); ++i)
    {
        const auto& m = m_modelData.materials[i];
        MaterialCB cb{};
        cb.diffuse = m.diffuse;
        cb.specular = m.specular;
        cb.specularPower = m.specularPower;
        cb.ambient = m.ambient;
        cb.emissive = m.emissive;
        m_materialCB->update(cb, i);
    }
}

void ModelRender::createTextures()
{
    auto addTexture = [&](const std::string& path)
        {
            if (path.empty()) return;

            std::wstring wpath(path.begin(), path.end());

            for (const auto& existing : m_texturePaths)
            {
                if (existing == wpath) return;
            }

            auto texture = TextureManager::Instance().load(wpath);
            if (texture)
            {
                m_textures.push_back(texture);
                m_texturePaths.push_back(wpath);
            }
        };

    for (const auto& mat : m_modelData.materials)
    {
        addTexture(mat.diffuseTexPath);
        addTexture(mat.normalTexPath);
        addTexture(mat.toonTexPath);
    }
}

void ModelRender::createPSO()
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
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDEX",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_psoKey = PSOCreator::Instance().registerPSO(psoData);
}

void ModelRender::rebuildSubsetDescriptors(Subset& subset)
{
    if (subset.descriptorBase == UINT_MAX) return;

    std::vector<UINT> srvIndices;

    for (UINT i = 0; i < static_cast<UINT>(TextureType::Max); ++i)
    {
        int texIdx = subset.textureIndices[i];

        if (texIdx >= 0 && texIdx < (int)m_textures.size())
            srvIndices.push_back(m_textures[texIdx]->getSRVIndex());
        else if (!m_textures.empty())
            srvIndices.push_back(m_textures[0]->getSRVIndex());
        else
            srvIndices.push_back(0);
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subset.descriptorBase, srvIndices);
}

void ModelRender::loadSetting()
{
    std::ifstream file(m_settingPath, std::ios::binary);
    if (!file) return;

    size_t totalSubsetCount = 0;
    file.read(reinterpret_cast<char*>(&totalSubsetCount), sizeof(size_t));

    size_t currentTotal = 0;
    for (const auto& mesh : m_meshes)
        currentTotal += mesh.subsets.size();

    if (!file || totalSubsetCount != currentTotal) return;

    for (auto& mesh : m_meshes)
    {
        for (auto& subset : mesh.subsets)
        {
            uint8_t visible = 1;
            file.read(reinterpret_cast<char*>(&visible), sizeof(uint8_t));
            subset.visible = (visible != 0);

            for (UINT t = 0; t < static_cast<UINT>(TextureType::Max); ++t)
            {
                size_t len = 0;
                file.read(reinterpret_cast<char*>(&len), sizeof(size_t));
                if (!file) return;

                if (len == 0) { subset.textureIndices[t] = -1; continue; }

                std::wstring path(len, L'\0');
                file.read(reinterpret_cast<char*>(path.data()), len * sizeof(wchar_t));
                if (!file) return;

                int foundIndex = -1;
                for (size_t k = 0; k < m_texturePaths.size(); ++k)
                {
                    if (m_texturePaths[k] == path) { foundIndex = (int)k; break; }
                }

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

            rebuildSubsetDescriptors(subset);
        }
    }
}

void ModelRender::saveSetting()
{
    std::ofstream file(m_settingPath, std::ios::binary | std::ios::trunc);
    if (!file) return;

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

            for (UINT t = 0; t < static_cast<UINT>(TextureType::Max); ++t)
            {
                int texIndex = subset.textureIndices[t];
                std::wstring path;
                if (texIndex >= 0 && texIndex < (int)m_texturePaths.size())
                    path = m_texturePaths[texIndex];

                size_t len = path.size();
                file.write(reinterpret_cast<const char*>(&len), sizeof(size_t));
                if (len > 0)
                    file.write(reinterpret_cast<const char*>(path.data()), len * sizeof(wchar_t));
            }
        }
    }
}

void ModelRender::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    render(cmd);
}

void ModelRender::render(ID3D12GraphicsCommandList* cmd)
{
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));
    PSOCreator::Instance().setPSO(m_psoKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    Matrix world = Matrix::Identity;
    if (m_transform)
        world = m_transform->getWorldMatrix();

    ModelCB modelCBData{};
    modelCBData.world = world.Transpose();
    m_modelCB->update(modelCBData);

    cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

    for (const auto& mesh : m_meshes)
    {
        mesh.vertexBuffer->bind(cmd);
        mesh.indexBuffer->bind(cmd);

        for (const auto& subset : mesh.subsets)
        {
            if (!subset.visible) continue;

            cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));
            cmd->SetGraphicsRootDescriptorTable(2, m_materialCB->getGPUHandle(subset.materialIndex));
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}

void ModelRender::debugRender()
{
    if (!ImGui::Begin("Model Material Editor"))
    {
        ImGui::End();
        return;
    }

    static int selectedMesh = -1;
    static int selectedSubset = -1;

    ImGui::Columns(2, nullptr, true);

    //! Subset List
    ImGui::BeginChild("SubsetList", ImVec2(0, 0), true);

    for (int mi = 0; mi < (int)m_meshes.size(); ++mi)
    {
        std::string meshLabel = (mi < (int)m_modelData.meshes.size() && !m_modelData.meshes[mi].name.empty())
            ? m_modelData.meshes[mi].name
            : "Mesh " + std::to_string(mi);

        if (ImGui::TreeNode(meshLabel.c_str()))
        {
            for (int si = 0; si < (int)m_meshes[mi].subsets.size(); ++si)
            {
                UINT matIdx = m_meshes[mi].subsets[si].materialIndex;
                std::string name = (matIdx < m_modelData.materials.size() && !m_modelData.materials[matIdx].name.empty())
                    ? m_modelData.materials[matIdx].name
                    : "Subset " + std::to_string(si);

                bool isSelected = (selectedMesh == mi && selectedSubset == si);

                if (ImGui::Selectable(name.c_str(), isSelected))
                {
                    selectedMesh = mi;
                    selectedSubset = si;
                }
            }

            ImGui::TreePop();
        }
    }

    ImGui::EndChild();
    ImGui::NextColumn();

    //! Inspector
    ImGui::BeginChild("Inspector", ImVec2(0, 0), true);

    if (selectedMesh >= 0 && selectedMesh < (int)m_meshes.size() &&
        selectedSubset >= 0 && selectedSubset < (int)m_meshes[selectedMesh].subsets.size())
    {
        auto& subset = m_meshes[selectedMesh].subsets[selectedSubset];
        UINT matIdx = subset.materialIndex;

        std::string matName = (matIdx < m_modelData.materials.size())
            ? (m_modelData.materials[matIdx].name.empty() ? "Material " + std::to_string(matIdx) : m_modelData.materials[matIdx].name)
            : "Unknown";

        ImGui::Text("Material : %s", matName.c_str());
        ImGui::Separator();

        if (ImGui::Checkbox("Visible", &subset.visible))
            saveSetting();

        //! マテリアルパラメータ編集
        if (matIdx < m_modelData.materials.size())
        {
            auto& mat = m_modelData.materials[matIdx];

            ImGui::Spacing();
            ImGui::Text("Properties");
            ImGui::Separator();

            bool changed = false;
            changed |= ImGui::ColorEdit4("Diffuse", &mat.diffuse.x);
            changed |= ImGui::ColorEdit3("Specular", &mat.specular.x);
            changed |= ImGui::DragFloat("Spec Power", &mat.specularPower, 0.1f, 0.0f, 256.0f);
            changed |= ImGui::ColorEdit3("Ambient", &mat.ambient.x);
            changed |= ImGui::ColorEdit3("Emissive", &mat.emissive.x);

            if (changed)
            {
                MaterialCB cb{};
                cb.diffuse = mat.diffuse;
                cb.specular = mat.specular;
                cb.specularPower = mat.specularPower;
                cb.ambient = mat.ambient;
                cb.emissive = mat.emissive;
                m_materialCB->update(cb, matIdx);
            }
        }

        ImGui::Spacing();
        ImGui::Text("Textures");

        if (ImGui::BeginTable("Textures", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            for (UINT i = 0; i < static_cast<UINT>(TextureType::Max); ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", magic_enum::enum_name(TextureType(i)).data());

                ImGui::TableSetColumnIndex(1);

                int texIndex = subset.textureIndices[i];

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