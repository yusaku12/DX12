#include "pch.h"
#include "FbxRender.h"
#include "Component\TransformComponent.h"

FbxRender::FbxRender(const std::string& fbxPath)
    : m_sourcePath(fbxPath)
{
    m_debugName = std::filesystem::path(fbxPath).filename().string();

    LOG_INFO("[FbxRender] Loading: %s", fbxPath.c_str());

    FbxLoad loader;
    if (!loader.load(fbxPath))
    {
        LOG_ERROR("[FbxRender] Failed to load FBX: %s", fbxPath.c_str());
        return;
    }

    m_modelData = ModelData::importFromFBX(loader.getModel());
    m_modelData.name = m_debugName;

    buildGPUResources();
    computeStatistics();
    m_valid = true;

    LOG_INFO("[FbxRender] Loaded: %s (meshes=%u, materials=%u, vertices=%u, triangles=%u)",
        fbxPath.c_str(),
        m_stats.meshCount,
        m_stats.materialCount,
        m_stats.totalVertices,
        m_stats.totalTriangles);
}

FbxRender::FbxRender(const FbxLoad::Model& fbxModel)
{
    m_debugName = "FbxRender (from Model)";

    m_modelData = ModelData::importFromFBX(fbxModel);

    buildGPUResources();
    computeStatistics();
    m_valid = true;

    LOG_INFO("[FbxRender] Built from FbxLoad::Model (meshes=%u, materials=%u, vertices=%u)",
        m_stats.meshCount,
        m_stats.materialCount,
        m_stats.totalVertices);
}

void FbxRender::buildGPUResources()
{
    //! モデル行列 CBV
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>();
    m_modelCB->update(ModelCB{});

    //! テクスチャ読み込み
    createTextures();

    //! メッシュ毎に VB / IB / Subsets を構築
    for (const auto& srcMesh : m_modelData.meshes)
    {
        MeshDrawData meshDraw;

        //! 頂点バッファ
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
                        std::wstring wpath = stringToWstring(path);
                        for (int i = 0; i < static_cast<int>(m_texturePaths.size()); ++i)
                        {
                            if (m_texturePaths[i] == wpath) return i;
                        }
                        return -1;
                    };

                s.textureIndices[static_cast<int>(TextureType::Diffuse)] = findTexIndex(mat.texturePath[0]);
                s.textureIndices[static_cast<int>(TextureType::Normal)] = findTexIndex(mat.texturePath[1]);
            }

            s.descriptorBase = DescriptorHeapManager::Instance().allocateRange(TEXTURE_SLOT_COUNT);
            rebuildSubsetDescriptors(s);

            meshDraw.subsets.push_back(s);
        }

        m_meshes.push_back(std::move(meshDraw));
    }

    //! マテリアル CBV
    createMaterialCBV();

    //! PSO（ソリッド + ワイヤーフレーム）
    createSolidPSO();
    createWireframePSO();
}

void FbxRender::createMaterialCBV()
{
    UINT matCount = static_cast<UINT>(m_modelData.materials.size());
    if (matCount == 0) matCount = 1;

    m_materialCB = std::make_unique<ConstantBuffer<MaterialCB>>(matCount);

    for (UINT i = 0; i < static_cast<UINT>(m_modelData.materials.size()); ++i)
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

void FbxRender::createTextures()
{
    auto addTexture = [&](const std::string& path)
        {
            if (path.empty()) return;

            std::wstring wpath = stringToWstring(path);

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
            else
            {
                LOG_WARN("[FbxRender] Failed to load texture: %s", path.c_str());
            }
        };

    for (const auto& mat : m_modelData.materials)
    {
        addTexture(mat.texturePath[0]);
        addTexture(mat.texturePath[1]);
    }
}

void FbxRender::createSolidPSO()
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
        //{ "BONEINDEX",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_solidPSOKey = PSOCreator::Instance().registerPSO(psoData);
}

void FbxRender::createWireframePSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::PMXStandard;
    psoData.vsShaderId = ShaderID::FBXVS;
    psoData.psShaderId = ShaderID::FBXPS;
    psoData.rasterizerState = RasterizerState::WIRE_FRAME;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEINDEX",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_wireframePSOKey = PSOCreator::Instance().registerPSO(psoData);
}

void FbxRender::rebuildSubsetDescriptors(Subset& subset)
{
    if (subset.descriptorBase == UINT_MAX) return;

    std::vector<UINT> srvIndices;
    srvIndices.reserve(TEXTURE_SLOT_COUNT);

    for (UINT i = 0; i < TEXTURE_SLOT_COUNT; ++i)
    {
        int texIdx = subset.textureIndices[i];

        if (texIdx >= 0 && texIdx < static_cast<int>(m_textures.size()))
        {
            srvIndices.push_back(m_textures[texIdx]->getSRVIndex());
        }
        else
        {
            //! テクスチャがない場合はフォールバック
            if (!m_textures.empty())
                srvIndices.push_back(m_textures[0]->getSRVIndex());
            else
                srvIndices.push_back(0);
        }
    }

    DescriptorHeapManager::Instance().copyDescriptorsRange(subset.descriptorBase, srvIndices);
}

void FbxRender::computeStatistics()
{
    m_stats = {};
    m_stats.meshCount = static_cast<uint32_t>(m_modelData.meshes.size());
    m_stats.materialCount = static_cast<uint32_t>(m_modelData.materials.size());

    for (const auto& mesh : m_modelData.meshes)
    {
        m_stats.totalVertices += static_cast<uint32_t>(mesh.vertices.size());
        m_stats.totalIndices += static_cast<uint32_t>(mesh.indices.size());
        m_stats.subMeshCount += static_cast<uint32_t>(mesh.subMeshes.size());
    }

    m_stats.totalTriangles = m_stats.totalIndices / 3;
}

void FbxRender::render()
{
    if (!m_valid) return;
    auto cmd = DX12::Instance().getGraphicsCommandList();
    render(cmd);
}

void FbxRender::render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_valid) return;

    switch (m_debugMode)
    {
    case DebugMode::Wireframe:
        renderInternal(cmd, m_wireframePSOKey);
        break;

    case DebugMode::WireframeOverlay:
        //! ソリッド → ワイヤーフレームの順に重ねて描画
        renderInternal(cmd, m_solidPSOKey);
        renderInternal(cmd, m_wireframePSOKey);
        break;

    case DebugMode::Normals:
        renderInternal(cmd, m_solidPSOKey);
        debugDrawNormals(m_normalDisplayLength);
        break;

    case DebugMode::Tangents:
        renderInternal(cmd, m_solidPSOKey);
        debugDrawTangents(m_tangentDisplayLength);
        break;

    case DebugMode::None:
    case DebugMode::UVChecker:
    case DebugMode::BoneWeights:
    default:
        renderInternal(cmd, m_solidPSOKey);
        break;
    }

    //! AABB 表示
    if (m_showBounds)
    {
        debugDrawBounds();
    }
}

void FbxRender::renderInternal(ID3D12GraphicsCommandList* cmd, size_t psoKey)
{
    m_stats.drawCallCount = 0;

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));
    PSOCreator::Instance().setPSO(psoKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    //! ワールド行列更新
    Matrix world = Matrix::Identity;
    if (m_transform)
        world = m_transform->getWorldMatrix();

    ModelCB modelCBData{};
    modelCBData.world = world.Transpose();
    m_modelCB->update(modelCBData);

    cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

    for (size_t meshIdx = 0; meshIdx < m_meshes.size(); ++meshIdx)
    {
        const auto& mesh = m_meshes[meshIdx];

        //! メッシュ単位の表示制御
        if (!mesh.visible) continue;

        mesh.vertexBuffer->bind(cmd);
        mesh.indexBuffer->bind(cmd);

        for (const auto& subset : mesh.subsets)
        {
            //! サブセット単位の表示制御
            if (!subset.visible) continue;

            //! マテリアル単位の表示制御
            if (subset.materialIndex < m_modelData.materials.size())
            {
                if (!m_modelData.materials[subset.materialIndex].isVisible)
                    continue;
            }

            cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));
            cmd->SetGraphicsRootDescriptorTable(2, m_materialCB->getGPUHandle(subset.materialIndex));
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
            ++m_stats.drawCallCount;
        }
    }
}

void FbxRender::debugDrawNormals(float length) const
{
    Matrix world = Matrix::Identity;
    if (m_transform)
        world = m_transform->getWorldMatrix();

    const Vector4 normalColor = { 0.0f, 0.5f, 1.0f, 1.0f };

    for (size_t meshIdx = 0; meshIdx < m_modelData.meshes.size(); ++meshIdx)
    {
        if (meshIdx < m_meshes.size() && !m_meshes[meshIdx].visible)
            continue;

        const auto& mesh = m_modelData.meshes[meshIdx];

        //! 全頂点の法線を描画すると重いので間引き
        size_t step = std::max<size_t>(1, mesh.vertices.size() / 2000);

        for (size_t i = 0; i < mesh.vertices.size(); i += step)
        {
            const auto& v = mesh.vertices[i];
            Vector3 start = Vector3::Transform(v.position, world);
            Vector3 end = Vector3::Transform(v.position + v.normal * length, world);

            Matrix lineWorld = Matrix::CreateTranslation(start);
            DebugPrimitive::Instance().drawSphere(lineWorld, length * 0.05f, normalColor);

            Matrix endWorld = Matrix::CreateTranslation(end);
            DebugPrimitive::Instance().drawSphere(endWorld, length * 0.05f, normalColor);
        }
    }
}

void FbxRender::debugDrawTangents(float length) const
{
    Matrix world = Matrix::Identity;
    if (m_transform)
        world = m_transform->getWorldMatrix();

    const Vector4 tangentColor = { 1.0f, 0.5f, 0.0f, 1.0f };

    for (size_t meshIdx = 0; meshIdx < m_modelData.meshes.size(); ++meshIdx)
    {
        if (meshIdx < m_meshes.size() && !m_meshes[meshIdx].visible)
            continue;

        const auto& mesh = m_modelData.meshes[meshIdx];

        //! 間引き
        size_t step = std::max<size_t>(1, mesh.vertices.size() / 2000);

        for (size_t i = 0; i < mesh.vertices.size(); i += step)
        {
            const auto& v = mesh.vertices[i];
            Vector3 tangentDir = Vector3(v.tangent.x, v.tangent.y, v.tangent.z);
            Vector3 start = Vector3::Transform(v.position, world);
            Vector3 end = Vector3::Transform(v.position + tangentDir * length, world);

            Matrix lineWorld = Matrix::CreateTranslation(start);
            DebugPrimitive::Instance().drawSphere(lineWorld, length * 0.05f, tangentColor);

            Matrix endWorld = Matrix::CreateTranslation(end);
            DebugPrimitive::Instance().drawSphere(endWorld, length * 0.05f, tangentColor);
        }
    }
}

void FbxRender::debugDrawBounds() const
{
    Matrix world = Matrix::Identity;
    if (m_transform)
        world = m_transform->getWorldMatrix();

    const Vector4 boundsColor = { 0.0f, 1.0f, 0.0f, 1.0f };
    const Vector4 meshBoundsColor = { 1.0f, 1.0f, 0.0f, 0.5f };

    //! 全体 AABB
    {
        Vector3 center = (m_modelData.boundsMin + m_modelData.boundsMax) * 0.5f;
        Vector3 extents = (m_modelData.boundsMax - m_modelData.boundsMin) * 0.5f;
        Matrix boxWorld = Matrix::CreateTranslation(center) * world;
        DebugPrimitive::Instance().drawBox(boxWorld, extents, boundsColor);
    }

    //! メッシュ個別の AABB
    for (size_t i = 0; i < m_modelData.meshes.size(); ++i)
    {
        if (i < m_meshes.size() && !m_meshes[i].visible)
            continue;

        const auto& mesh = m_modelData.meshes[i];
        Vector3 center = (mesh.boundsMin + mesh.boundsMax) * 0.5f;
        Vector3 extents = (mesh.boundsMax - mesh.boundsMin) * 0.5f;
        Matrix boxWorld = Matrix::CreateTranslation(center) * world;
        DebugPrimitive::Instance().drawBox(boxWorld, extents, meshBoundsColor);
    }
}

void FbxRender::setMeshVisible(uint32_t meshIndex, bool visible)
{
    if (meshIndex < m_meshes.size())
        m_meshes[meshIndex].visible = visible;
}

bool FbxRender::getMeshVisible(uint32_t meshIndex) const
{
    if (meshIndex < m_meshes.size())
        return m_meshes[meshIndex].visible;
    return false;
}

void FbxRender::setMaterialVisible(uint32_t materialIndex, bool visible)
{
    if (materialIndex < m_modelData.materials.size())
        m_modelData.materials[materialIndex].isVisible = visible;
}

bool FbxRender::getMaterialVisible(uint32_t materialIndex) const
{
    if (materialIndex < m_modelData.materials.size())
        return m_modelData.materials[materialIndex].isVisible;
    return false;
}

void FbxRender::rebuild()
{
    LOG_INFO("[FbxRender] Rebuilding GPU resources...");

    //! GPU が使用中のリソースを安全に解放するため、全コマンド完了を待つ
    DX12::Instance().safeGPUWait();

    //! 既存リソースをクリア
    m_meshes.clear();
    m_textures.clear();
    m_texturePaths.clear();
    m_materialCB.reset();
    m_modelCB.reset();

    //! AABB 再計算
    m_modelData.computeBounds();

    //! GPU リソース再構築
    buildGPUResources();
    computeStatistics();

    LOG_INFO("[FbxRender] Rebuild complete (meshes=%u, vertices=%u, triangles=%u)",
        m_stats.meshCount, m_stats.totalVertices, m_stats.totalTriangles);
}

void FbxRender::debugImGui()
{
    if (!ImGui::Begin(std::format("FbxRender: {}", m_debugName).c_str()))
    {
        ImGui::End();
        return;
    }

    //! ステータスバー
    if (m_valid)
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), reinterpret_cast<const char*>(u8"[有効]"));
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), reinterpret_cast<const char*>(u8"[無効]"));
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Source: %s", m_sourcePath.c_str());
    ImGui::Separator();

    //! タブバー
    if (ImGui::BeginTabBar("FbxRenderTabs"))
    {
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"統計")))
        {
            imguiStatisticsPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"メッシュ")))
        {
            imguiMeshPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"マテリアル")))
        {
            imguiMaterialPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"デバッグ")))
        {
            imguiDebugPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"エクスポート")))
        {
            imguiExportPanel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void FbxRender::imguiStatisticsPanel()
{
    ImGui::Text(reinterpret_cast<const char*>(u8"モデル名: %s"), m_modelData.name.c_str());
    ImGui::Separator();

    ImGui::Columns(2, "StatsColumns", true);
    ImGui::SetColumnWidth(0, 150.0f);

    ImGui::Text(reinterpret_cast<const char*>(u8"メッシュ数"));        ImGui::NextColumn(); ImGui::Text("%u", m_stats.meshCount);        ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"マテリアル数"));      ImGui::NextColumn(); ImGui::Text("%u", m_stats.materialCount);    ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"サブメッシュ数"));    ImGui::NextColumn(); ImGui::Text("%u", m_stats.subMeshCount);     ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"頂点数"));            ImGui::NextColumn(); ImGui::Text("%u", m_stats.totalVertices);    ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"インデックス数"));    ImGui::NextColumn(); ImGui::Text("%u", m_stats.totalIndices);     ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"三角形数"));          ImGui::NextColumn(); ImGui::Text("%u", m_stats.totalTriangles);   ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"ドローコール数"));    ImGui::NextColumn(); ImGui::Text("%u", m_stats.drawCallCount);    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("AABB Min: (%.3f, %.3f, %.3f)", m_modelData.boundsMin.x, m_modelData.boundsMin.y, m_modelData.boundsMin.z);
    ImGui::Text("AABB Max: (%.3f, %.3f, %.3f)", m_modelData.boundsMax.x, m_modelData.boundsMax.y, m_modelData.boundsMax.z);

    Vector3 size = m_modelData.boundsMax - m_modelData.boundsMin;
    ImGui::Text("AABB Size: (%.3f, %.3f, %.3f)", size.x, size.y, size.z);
}

void FbxRender::imguiMeshPanel()
{
    for (size_t i = 0; i < m_meshes.size(); ++i)
    {
        auto& meshDraw = m_meshes[i];
        const auto& meshData = m_modelData.meshes[i];

        std::string label = std::format("[{}] {}", i, meshData.name.empty() ? "Unnamed" : meshData.name);

        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        //! 表示 ON/OFF チェックボックス（ツリーノード横に配置）
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
        std::string checkId = std::format("##meshVis{}", i);
        ImGui::Checkbox(checkId.c_str(), &meshDraw.visible);

        if (nodeOpen)
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"  頂点数: %zu"), meshData.vertices.size());
            ImGui::Text(reinterpret_cast<const char*>(u8"  インデックス数: %zu"), meshData.indices.size());
            ImGui::Text(reinterpret_cast<const char*>(u8"  サブメッシュ数: %zu"), meshData.subMeshes.size());
            ImGui::Text("  AABB Min: (%.3f, %.3f, %.3f)", meshData.boundsMin.x, meshData.boundsMin.y, meshData.boundsMin.z);
            ImGui::Text("  AABB Max: (%.3f, %.3f, %.3f)", meshData.boundsMax.x, meshData.boundsMax.y, meshData.boundsMax.z);

            //! サブセット詳細
            for (size_t j = 0; j < meshDraw.subsets.size(); ++j)
            {
                auto& subset = meshDraw.subsets[j];
                std::string subLabel = std::format("  Subset [{}] mat={} idx={}-{}",
                    j, subset.materialIndex, subset.startIndex, subset.startIndex + subset.indexCount);

                ImGui::Indent();
                std::string subCheckId = std::format("##subVis{}_{}", i, j);
                ImGui::Checkbox(subCheckId.c_str(), &subset.visible);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", subLabel.c_str());
                ImGui::Unindent();
            }

            ImGui::TreePop();
        }
    }
}

void FbxRender::imguiMaterialPanel()
{
    //! rebuild() をループ中に呼ぶとクラッシュするので、フラグで遅延実行
    bool needsRebuild = false;

    //! マテリアルパラメータの CBV 更新用ヘルパー
    auto updateMaterialCB = [&](size_t index)
        {
            const auto& mat = m_modelData.materials[index];
            MaterialCB cb{};
            cb.diffuse = mat.diffuse;
            cb.specular = mat.specular;
            cb.specularPower = mat.specularPower;
            cb.ambient = mat.ambient;
            cb.emissive = mat.emissive;
            m_materialCB->update(cb, static_cast<UINT>(index));
        };

    for (size_t i = 0; i < m_modelData.materials.size(); ++i)
    {
        auto& mat = m_modelData.materials[i];

        std::string label = std::format("[{}] {}", i, mat.name.empty() ? "Unnamed" : mat.name);
        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        //! 表示 ON/OFF
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
        std::string checkId = std::format("##matVis{}", i);
        ImGui::Checkbox(checkId.c_str(), &mat.isVisible);

        if (nodeOpen)
        {
            //! Diffuse カラー編集
            float diffuse[4] = { mat.diffuse.x, mat.diffuse.y, mat.diffuse.z, mat.diffuse.w };
            if (ImGui::ColorEdit4(std::format("Diffuse##{}", i).c_str(), diffuse))
            {
                mat.diffuse = Vector4(diffuse[0], diffuse[1], diffuse[2], diffuse[3]);
                updateMaterialCB(i);
            }

            //! Specular
            float spec[3] = { mat.specular.x, mat.specular.y, mat.specular.z };
            if (ImGui::ColorEdit3(std::format("Specular##{}", i).c_str(), spec))
            {
                mat.specular = Vector3(spec[0], spec[1], spec[2]);
                updateMaterialCB(i);
            }

            //! Specular Power
            if (ImGui::DragFloat(std::format("Specular Power##{}", i).c_str(), &mat.specularPower, 0.1f, 0.0f, 256.0f))
            {
                updateMaterialCB(i);
            }

            //! Ambient
            float amb[3] = { mat.ambient.x, mat.ambient.y, mat.ambient.z };
            if (ImGui::ColorEdit3(std::format("Ambient##{}", i).c_str(), amb))
            {
                mat.ambient = Vector3(amb[0], amb[1], amb[2]);
                updateMaterialCB(i);
            }

            //! Emissive
            float emi[3] = { mat.emissive.x, mat.emissive.y, mat.emissive.z };
            if (ImGui::ColorEdit3(std::format("Emissive##{}", i).c_str(), emi))
            {
                mat.emissive = Vector3(emi[0], emi[1], emi[2]);
                updateMaterialCB(i);
            }

            //! テクスチャパス表示 & 張替ボタン
            ImGui::TextDisabled("Diffuse Tex: %s", mat.texturePath[0].empty() ? "(none)" : mat.texturePath[0].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(std::format("change##diffTex{}", i).c_str()))
            {
                std::vector<std::wstring> paths;
                auto result = Dialog::openFile(paths, L"Diffuse Texture");
                if (result == DialogResult::OK && !paths.empty())
                {
                    mat.texturePath[0] = toRelativePath(paths[0]);
                    needsRebuild = true;
                }
            }

            ImGui::TextDisabled("Normal Tex:  %s", mat.texturePath[1].empty() ? "(none)" : mat.texturePath[1].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(std::format("change##normTex{}", i).c_str()))
            {
                std::vector<std::wstring> paths;
                auto result = Dialog::openFile(paths, L"Normal Texture");
                if (result == DialogResult::OK && !paths.empty())
                {
                    mat.texturePath[1] = toRelativePath(paths[0]);
                    needsRebuild = true;
                }
            }

            ImGui::TreePop();
        }
    }

    //! ループ終了後に安全にリビルド
    if (needsRebuild)
    {
        rebuild();
    }
}

void FbxRender::imguiDebugPanel()
{
    //! デバッグモード選択
    static const char* debugModeNames[] = {
        reinterpret_cast<const char*>(u8"なし"),
        reinterpret_cast<const char*>(u8"ワイヤーフレーム"),
        reinterpret_cast<const char*>(u8"ワイヤーフレーム重畳"),
        reinterpret_cast<const char*>(u8"法線表示"),
        reinterpret_cast<const char*>(u8"接線表示"),
        reinterpret_cast<const char*>(u8"UV チェッカー"),
        reinterpret_cast<const char*>(u8"ボーンウェイト"),
    };

    int currentMode = static_cast<int>(m_debugMode);
    if (ImGui::Combo(reinterpret_cast<const char*>(u8"デバッグモード"), &currentMode, debugModeNames, IM_ARRAYSIZE(debugModeNames)))
    {
        m_debugMode = static_cast<DebugMode>(currentMode);
    }

    ImGui::Separator();

    //! AABB 表示
    ImGui::Checkbox(reinterpret_cast<const char*>(u8"AABB 表示"), &m_showBounds);

    //! 法線・接線表示の長さ
    if (m_debugMode == DebugMode::Normals)
    {
        ImGui::SliderFloat(reinterpret_cast<const char*>(u8"法線の長さ"), &m_normalDisplayLength, 0.001f, 0.5f, "%.3f");
    }
    if (m_debugMode == DebugMode::Tangents)
    {
        ImGui::SliderFloat(reinterpret_cast<const char*>(u8"接線の長さ"), &m_tangentDisplayLength, 0.001f, 0.5f, "%.3f");
    }

    ImGui::Separator();

    //! 全メッシュ表示 ON/OFF
    if (ImGui::Button(reinterpret_cast<const char*>(u8"全メッシュ表示")))
    {
        for (auto& mesh : m_meshes)
            mesh.visible = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char*>(u8"全メッシュ非表示")))
    {
        for (auto& mesh : m_meshes)
            mesh.visible = false;
    }

    //! 全マテリアル表示 ON/OFF
    if (ImGui::Button(reinterpret_cast<const char*>(u8"全マテリアル表示")))
    {
        for (auto& mat : m_modelData.materials)
            mat.isVisible = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(reinterpret_cast<const char*>(u8"全マテリアル非表示")))
    {
        for (auto& mat : m_modelData.materials)
            mat.isVisible = false;
    }
}

void FbxRender::imguiExportPanel()
{
    ImGui::Text(reinterpret_cast<const char*>(u8"元ファイル: %s"), m_sourcePath.c_str());
    ImGui::Separator();

    //! モデル名編集
    static char nameBuffer[256] = {};
    if (nameBuffer[0] == '\0' && !m_modelData.name.empty())
    {
        strncpy_s(nameBuffer, m_modelData.name.c_str(), sizeof(nameBuffer) - 1);
    }

    if (ImGui::InputText(reinterpret_cast<const char*>(u8"モデル名"), nameBuffer, sizeof(nameBuffer)))
    {
        m_modelData.name = nameBuffer;
    }

    ImGui::Separator();

    //! .mdl エクスポート
    static char exportPath[512] = "output.mdl";
    ImGui::InputText(reinterpret_cast<const char*>(u8"出力パス (.mdl)"), exportPath, sizeof(exportPath));

    if (ImGui::Button(reinterpret_cast<const char*>(u8".mdl にエクスポート")))
    {
        if (m_modelData.saveToMdl(exportPath))
        {
            LOG_INFO("[FbxRender] Exported to: %s", exportPath);
        }
        else
        {
            LOG_ERROR("[FbxRender] Export failed: %s", exportPath);
        }
    }

    ImGui::Separator();

    //! リビルドボタン
    if (ImGui::Button(reinterpret_cast<const char*>(u8"GPU リソース再構築")))
    {
        rebuild();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(reinterpret_cast<const char*>(u8"(ModelData 編集後に実行)"));
}