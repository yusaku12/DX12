#include "pch.h"
#include "FbxRenderComponent.h"
#include "TransformComponent.h"

FbxRenderComponent::FbxRenderComponent(const std::string& fbxPath)
{
    // fbxを読み込み
    if (loadFbx(fbxPath))
    {
        LOG_INFO("[FbxRenderComponent] Loaded FBX: %s", fbxPath.c_str());
    }
    else
    {
        return;
    }

    // GPUリソース構築
    buildGPUResources();

    // 統計情報更新
    m_model->getResource()->computeStatistics();
}

void FbxRenderComponent::awake()
{
    m_transform = gameObject()->getComponent<TransformComponent>();
}

void FbxRenderComponent::update()
{
    // Transform が取得できていなければスキップ
    if (!m_transform) return;

    // モデル行列更新
    m_model->updateTransform(m_transform->getLocalMatrix());

    // AABB 描画
    if (m_showAABB)
    {
        renderAABB();
    }
}

void FbxRenderComponent::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    // コマンドリストが無効なら描画スキップ
    if (!cmd) return;
    render(cmd);
}

void FbxRenderComponent::render(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    switch (m_debugMode)
    {
    case DebugMode::Wireframe:
        renderInternal(cmd, m_wireframePSOKey);
        break;

    case DebugMode::None:
    default:
        renderInternal(cmd, m_solidPSOKey);
        break;
    }
}

void FbxRenderComponent::inspectGUI()
{
    if (ImGui::BeginTabBar("FBXRender"))
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
}

bool FbxRenderComponent::loadFbx(const std::string& fbxPath)
{
    auto fbx = std::make_unique<FbxLoad>();

    if (!fbx->load(fbxPath.c_str()))
    {
        LOG_ERROR("[FbxRender] Failed: %s", fbxPath.c_str());
        return false;
    }

    // 所有権をModelに移動
    m_model = std::make_unique<Model>(std::move(fbx));

    return true;
}

void FbxRenderComponent::buildGPUResources()
{
    // モデル行列 CBV
    UINT meshCount = static_cast<UINT>(m_model->getResource()->getModelData().meshes.size());
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>(meshCount);
    for (UINT i = 0; i < meshCount; ++i)
    {
        m_modelCB->update(ModelCB{}, i);
    }

    // テクスチャ読み込み
    m_model->getResource()->createTextures();

    // メッシュ構築
    m_model->getResource()->createMesh();

    // マテリアル CBV
    createMaterialCBV();

    // PSO（ソリッド + ワイヤーフレーム）
    createSolidPSO();
    createWireframePSO();
}

void FbxRenderComponent::createMaterialCBV()
{
    const auto& modelData = m_model->getResource()->getModelData();
    UINT matCount = static_cast<UINT>(modelData.materials.size());
    if (matCount == 0)
    {
        matCount = 1;
    }

    // マテリアル CBV をマテリアル数分確保（最低1つ）
    m_materialCB = std::make_unique<ConstantBuffer<MaterialCB>>(matCount);

    for (UINT i = 0; i < matCount; ++i)
    {
        MaterialCB cb{};
        if (i < static_cast<UINT>(modelData.materials.size()))
        {
            const auto& m = modelData.materials[i];
            cb.diffuse = m.diffuseColor;
        }
        else
        {
            // マテリアルが無い場合はデフォルト白
            cb.diffuse = Vector4{ 1.f, 1.f, 1.f, 1.f };
        }
        m_materialCB->update(cb, i);
    }
}

void FbxRenderComponent::createSolidPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::PMXStandard;
    psoData.vsShaderId = ShaderID::FBXVS;
    psoData.psShaderId = ShaderID::FBXPS;
    psoData.rasterizerState = RasterizerState::CULL_CLOCKWISE;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONES",      0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_solidPSOKey = PSOCreator::Instance().registerPSO(psoData);
}

void FbxRenderComponent::createWireframePSO()
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
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONES",      0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_wireframePSOKey = PSOCreator::Instance().registerPSO(psoData);
}

void FbxRenderComponent::renderInternal(ID3D12GraphicsCommandList* cmd, size_t psoKey)
{
    // PSO とルートシグネチャをセット
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));
    PSOCreator::Instance().setPSO(psoKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    const auto& modelData = m_model->getResource()->getModelData();

    for (size_t meshIdx = 0; meshIdx < modelData.meshes.size(); ++meshIdx)
    {
        const auto& mesh = modelData.meshes[meshIdx];

        // メッシュ単位の表示制御
        if (!mesh.visible) continue;

        // ModelCB を組み立てる
        ModelCB modelCBData{};

        // メッシュ用定数バッファ更新
        if (!mesh.nodeIndices.empty())
        {
            for (size_t i = 0; i < mesh.nodeIndices.size(); ++i)
            {
                Matrix worldTransform = m_model->getBone().at(mesh.nodeIndices.at(i)).worldTransform;
                Matrix offsetTransform = mesh.offsetTransforms.at(i);
                Matrix boneTransform = offsetTransform * worldTransform;
                modelCBData.boneTransforms[i] = boneTransform;
            }
        }
        else
        {
            modelCBData.boneTransforms[0] = m_model->getBone().at(mesh.nodeIndex).worldTransform;
        }

        // CBV 更新 & ルートにセット
        m_modelCB->update(modelCBData, static_cast<UINT>(meshIdx));
        cmd->SetGraphicsRootConstantBufferView(1, m_modelCB->getGPUAddress(static_cast<UINT>(meshIdx)));

        // メッシュバッファをセット（現在処理中のメッシュのみ）
        m_model->getResource()->bindGpuMesh(cmd, meshIdx);

        for (const auto& subset : mesh.subMeshes)
        {
            // サブセット単位の表示制御
            if (!subset.visible) continue;

            // マテリアル単位の表示制御
            if (subset.materialIndex < modelData.materials.size())
            {
                if (!modelData.materials[subset.materialIndex].visible)
                    continue;
            }

            // descriptorBase が無効ならそのサブセットは描画スキップ（不正なハンドルを渡さない）
            if (subset.descriptorBase == UINT_MAX)
            {
                LOG_WARN("[FbxRenderComponent] Skip subset with invalid descriptorBase");
                continue;
            }

            // material index が範囲外なら 0 にフォールバック
            UINT matIndex = 0;
            if (subset.materialIndex < modelData.materials.size())
                matIndex = static_cast<UINT>(subset.materialIndex);

            cmd->SetGraphicsRootConstantBufferView(2, m_materialCB->getGPUAddress(matIndex));
            cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}

void FbxRenderComponent::renderAABB()
{
    Vector3 modelWorldMin(std::numeric_limits<float>::max());
    Vector3 modelWorldMax(std::numeric_limits<float>::lowest());

    for (const auto& mesh : m_model->getResource()->getModelData().meshes)
    {
        Vector3 localMin(mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z);
        Vector3 localMax(mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z);

        Vector3 corners[8] =
        {
            { localMin.x, localMin.y, localMin.z },
            { localMax.x, localMin.y, localMin.z },
            { localMin.x, localMax.y, localMin.z },
            { localMax.x, localMax.y, localMin.z },
            { localMin.x, localMin.y, localMax.z },
            { localMax.x, localMin.y, localMax.z },
            { localMin.x, localMax.y, localMax.z },
            { localMax.x, localMax.y, localMax.z },
        };

        if (!mesh.nodeIndices.empty())
        {
            for (size_t ni = 0; ni < mesh.nodeIndices.size(); ++ni)
            {
                int nodeIdx = mesh.nodeIndices[ni];
                Matrix boneWorld = m_model->getBone().at(nodeIdx).worldTransform;
                Matrix combined = mesh.offsetTransforms[ni] * boneWorld;

                for (int c = 0; c < 8; ++c)
                {
                    Vector3 w = Vector3::Transform(corners[c], combined);

                    modelWorldMin = Vector3::Min(modelWorldMin, w);
                    modelWorldMax = Vector3::Max(modelWorldMax, w);
                }
            }
        }
        else
        {
            Matrix boneWorld = m_model->getBone().at(mesh.nodeIndex).worldTransform;

            for (int c = 0; c < 8; ++c)
            {
                Vector3 w = Vector3::Transform(corners[c], boneWorld);

                modelWorldMin = Vector3::Min(modelWorldMin, w);
                modelWorldMax = Vector3::Max(modelWorldMax, w);
            }
        }
    }

    if (modelWorldMin.x > modelWorldMax.x ||
        modelWorldMin.y > modelWorldMax.y ||
        modelWorldMin.z > modelWorldMax.z)
    {
        return;
    }

    Vector3 center = (modelWorldMin + modelWorldMax) * 0.5f;
    Vector3 extents = (modelWorldMax - modelWorldMin) * 0.5f;
    Matrix boxWorld = Matrix::CreateTranslation(center);
    Vector4 color = Vector4{ 1.0f, 1.0f, 0.0f, 1.0f };

    DebugPrimitive::Instance().drawBox(boxWorld, extents, color);
}

void FbxRenderComponent::imguiStatisticsPanel()
{
    ImGui::Columns(2, "StatsColumns", true);
    ImGui::SetColumnWidth(0, 150.0f);

    auto& status = m_model->getResource()->getStatistics();
    ImGui::Text(reinterpret_cast<const char*>(u8"メッシュ数"));        ImGui::NextColumn(); ImGui::Text("%u", status.meshCount);        ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"マテリアル数"));      ImGui::NextColumn(); ImGui::Text("%u", status.materialCount);    ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"サブメッシュ数"));    ImGui::NextColumn(); ImGui::Text("%u", status.subMeshCount);     ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"頂点数"));            ImGui::NextColumn(); ImGui::Text("%u", status.totalVertices);    ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"インデックス数"));    ImGui::NextColumn(); ImGui::Text("%u", status.totalIndices);     ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"三角形数"));          ImGui::NextColumn(); ImGui::Text("%u", status.totalTriangles);   ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"ドローコール数"));    ImGui::NextColumn(); ImGui::Text("%u", status.drawCallCount);    ImGui::NextColumn();

    ImGui::Columns(1);
}

void FbxRenderComponent::imguiMeshPanel()
{
    for (size_t i = 0; i < m_model->getResource()->getModelData().meshes.size(); ++i)
    {
        auto meshDraw = m_model->getResource()->getModelData().meshes[i];
        const auto& meshData = m_model->getResource()->getModelData().meshes[i];

        std::string label = std::format("[{}] {}", i, meshData.name.empty() ? "Unnamed" : meshData.name);

        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        // 表示 ON/OFF チェックボックス（ツリーノード横に配置）
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
        std::string checkId = std::format("##meshVis{}", i);
        ImGui::Checkbox(checkId.c_str(), &meshDraw.visible);

        if (nodeOpen)
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"  頂点数: %zu"), meshData.vertices.size());
            ImGui::Text(reinterpret_cast<const char*>(u8"  インデックス数: %zu"), meshData.indices.size());
            ImGui::Text(reinterpret_cast<const char*>(u8"  サブメッシュ数: %zu"), meshData.subMeshes.size());

            // サブセット詳細
            for (size_t j = 0; j < meshDraw.subMeshes.size(); ++j)
            {
                auto& subset = meshDraw.subMeshes[j];
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

void FbxRenderComponent::imguiMaterialPanel()
{
}

void FbxRenderComponent::imguiDebugPanel()
{
    // デバッグモード選択
    static const char* debugModeNames[] =
    {
        reinterpret_cast<const char*>(u8"通常描画"),
        reinterpret_cast<const char*>(u8"ワイヤーフレーム"),
    };

    int currentMode = static_cast<int>(m_debugMode);
    if (ImGui::Combo(reinterpret_cast<const char*>(u8"デバッグモード"), &currentMode, debugModeNames, IM_ARRAYSIZE(debugModeNames)))
    {
        m_debugMode = static_cast<DebugMode>(currentMode);
    }
    ImGui::Checkbox(reinterpret_cast<const char*>(u8"AABB表示"), &m_showAABB);
}

void FbxRenderComponent::imguiExportPanel()
{
}