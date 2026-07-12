#include "pch.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Render/GBufferRenderTargets.h"
#include "SSREffect.h"

void SSREffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::SSRPS, RootSignatureType::PostEffectGBuffer);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void SSREffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
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
    cb.params0 = Vector4(
        std::max(1.0f, m_maxDistance),
        std::max(0.01f, m_thickness),
        std::clamp(m_stride, 0.25f, 2.0f),
        std::clamp(m_intensity, 0.0f, 2.0f));
    cb.params1 = Vector4(
        std::max(0.001f, camera->getNear()),
        std::max(camera->getNear() + 0.001f, camera->getFar()),
        static_cast<float>(std::clamp(m_maxSteps, 8, 128)),
        std::clamp(m_blendWeight, 0.0f, 1.0f));
    cb.params2 = Vector4(
        std::clamp(m_fresnelBias, 0.0f, 1.0f),
        std::clamp(m_fresnelPower, 0.5f, 10.0f),
        std::clamp(m_roughnessCutoff, 0.0f, 1.0f),
        std::clamp(m_edgeFade, 0.0f, 0.45f));

    m_cb->update(cb);

    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex()));
    cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(GBufferRenderTargets::Instance().getSrvIndex(1)));

    drawFullscreenTriangle(cmd);
}

void SSREffect::inspectGUI()
{
    ImGui::SeparatorText("SSR");
    ImGui::DragFloat("Max Distance", &m_maxDistance, 0.25f, 1.0f, 100.0f);
    ImGui::DragFloat("Thickness", &m_thickness, 0.005f, 0.01f, 1.0f);
    ImGui::SliderFloat("Stride", &m_stride, 0.25f, 2.0f);
    ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 2.0f);
    ImGui::SliderInt("Max Steps", &m_maxSteps, 8, 128);
    ImGui::SliderFloat("Fresnel Bias", &m_fresnelBias, 0.0f, 1.0f);
    ImGui::SliderFloat("Fresnel Power", &m_fresnelPower, 0.5f, 10.0f);
    ImGui::SliderFloat("Roughness Cutoff", &m_roughnessCutoff, 0.0f, 1.0f);
    ImGui::SliderFloat("Edge Fade", &m_edgeFade, 0.0f, 0.45f);
}