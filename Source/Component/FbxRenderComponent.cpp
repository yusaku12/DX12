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
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>();
    m_modelCB->update(ModelCB{});

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
    if (!cmd) return;
    if (!m_model) return;
    if (!m_modelCB) return;
    if (!m_materialCB) return;

    // PSO とルートシグネチャをセット
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::PMXStandard));
    PSOCreator::Instance().setPSO(psoKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    const auto& modelData = m_model->getResource()->getModelData();

    for (size_t meshIdx = 0; meshIdx < modelData.meshes.size(); ++meshIdx)
    {
        const auto& mesh = modelData.meshes[meshIdx];

        // メッシュ単位の表示制御
        if (!mesh.visible) continue;

        // ModelCB を組み立てる
        ModelCB modelCBData{};

        // メッシュ用定数バッファ更新
        if (mesh.nodeIndices.size() > 0)
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
        m_modelCB->update(modelCBData);
        cmd->SetGraphicsRootDescriptorTable(1, m_modelCB->getGPUHandle());

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

            cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));
            cmd->SetGraphicsRootDescriptorTable(2, m_materialCB->getGPUHandle(matIndex));
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}