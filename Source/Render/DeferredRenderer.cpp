#include "pch.h"

void DeferredRenderer::initialize()
{
    m_lightCB = std::make_unique<ConstantBuffer<LightParams>>();
    m_lightCB->update(m_lightParams);

    PSOCreator::PSOData pso{};
    pso.rootSignatureType = RootSignatureType::DeferredLighting;
    pso.vsShaderId = ShaderID::PostEffectVS;
    pso.psShaderId = ShaderID::DeferredLightingPS;
    pso.rasterizerState = RasterizerState::CULL_NONE;
    pso.blendState = BlendState::OPAQUE;
    pso.depthStencilState = DepthStencilState::DEPTH_NONE;
    pso.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.numRenderTargets = 1;
    pso.rtvFormats[0] = DX12::Instance().getBackBufferFormat();

    m_lightingPsoKey = PSOCreator::Instance().registerPSO(pso);
}

void DeferredRenderer::resize(UINT width, UINT height)
{
    GBufferRenderTargets::Instance().resize(width, height);
}

void DeferredRenderer::renderLighting()
{
    auto* cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    auto& gbuffer = GBufferRenderTargets::Instance();
    const auto srvBaseIndex = gbuffer.getSrvBaseIndex();

    if (srvBaseIndex == UINT_MAX)
    {
        LOG_ERROR("DeferredRenderer: GBuffer SRV is not initialized");
        return;
    }

    gbuffer.transitionToSRV(cmd);

    DX12::Instance().transitionSceneToRenderTarget();
    DX12::Instance().applyViewportAndScissor(cmd);
    DX12::Instance().applySceneRenderTargets(cmd);

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    m_lightCB->update(m_lightParams);

    PSOCreator::Instance().setPSO(m_lightingPsoKey, cmd);
    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());
    cmd->SetGraphicsRootConstantBufferView(1, m_lightCB->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(srvBaseIndex));

    // IBL ディスクリプタをセット
    auto iblHandle = IBLManager::Instance().getDescriptorHandle();
    if (iblHandle.ptr != 0)
    {
        cmd->SetGraphicsRootDescriptorTable(3, iblHandle);
    }

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->IASetIndexBuffer(nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);
}