#include "pch.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Graphics/IBLManager.h"
#include "Render/RayTracingRenderer.h"
#include "Render/GBufferRenderTargets.h"
#include "HybridGIEffect.h"

void HybridGIEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::HybridGIPS, RootSignatureType::PostEffectGBufferIBLRT);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void HybridGIEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    auto* camera = CameraManager::Instance().getMainCamera();
    if (!cmd || !camera || inputSrvIndex == UINT_MAX)
    {
        return;
    }

    CBuffer cb{};
    cb.projection = camera->getProjection();
    cb.invProjection = cb.projection.Invert();
    cb.view = camera->getView();
    cb.invView = cb.view.Invert();
    cb.cameraNearFar = Vector4(
        std::max(0.001f, camera->getNear()),
        std::max(camera->getNear() + 0.001f, camera->getFar()),
        0.0f,
        0.0f);
    cb.params0 = Vector4(
        std::clamp(m_intensity, 0.0f, 3.0f),
        std::clamp(m_ssgiWeight, 0.0f, 1.0f),
        std::clamp(m_probeWeight, 0.0f, 1.0f),
        std::max(0.25f, m_maxDistance));
    cb.params1 = Vector4(
        std::max(0.01f, m_thickness),
        std::clamp(m_stepStride, 0.2f, 2.0f),
        static_cast<float>(std::clamp(m_maxSteps, 4, 64)),
        static_cast<float>(std::clamp(m_hemisphereSamples, 1, 16)));
    cb.params2 = Vector4(
        std::clamp(m_normalBias, 0.0f, 0.25f),
        std::clamp(m_temporalStableJitter, 0.0f, 1.0f),
        std::clamp(m_blendWeight, 0.0f, 1.0f),
        0.0f);

    auto iblHandle = IBLManager::Instance().getDescriptorHandle();
    if (iblHandle.ptr == 0)
    {
        cb.params0.z = 0.0f;
    }

    m_cb->update(cb);

    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex()));
    cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(GBufferRenderTargets::Instance().getSrvIndex(1)));

    if (iblHandle.ptr != 0)
    {
        cmd->SetGraphicsRootDescriptorTable(4, iblHandle);
    }

    const UINT rtSrv = RayTracingRenderer::Instance().getOutputSrvIndex();
    const UINT rtInput = (rtSrv != UINT_MAX) ? rtSrv : inputSrvIndex;
    cmd->SetGraphicsRootDescriptorTable(5, DescriptorHeapManager::Instance().getGPUHandle(rtInput));

    drawFullscreenTriangle(cmd);
}

void HybridGIEffect::inspectGUI()
{
    ImGui::SeparatorText("Hybrid GI (DDGI/SSGI)");
    ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 3.0f);
    ImGui::SliderFloat("SSGI Weight", &m_ssgiWeight, 0.0f, 1.0f);
    ImGui::SliderFloat("Probe Weight", &m_probeWeight, 0.0f, 1.0f);
    ImGui::DragFloat("Max Distance", &m_maxDistance, 0.25f, 0.25f, 40.0f);
    ImGui::DragFloat("Thickness", &m_thickness, 0.005f, 0.01f, 1.0f);
    ImGui::SliderFloat("Step Stride", &m_stepStride, 0.2f, 2.0f);
    ImGui::SliderInt("Max Steps", &m_maxSteps, 4, 64);
    ImGui::SliderInt("Hemisphere Samples", &m_hemisphereSamples, 1, 16);
    ImGui::SliderFloat("Normal Bias", &m_normalBias, 0.0f, 0.25f);
    ImGui::SliderFloat("Stable Jitter", &m_temporalStableJitter, 0.0f, 1.0f);
}
