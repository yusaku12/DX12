#include "pch.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Render/GBufferRenderTargets.h"
#include "SSGIEffect.h"

void SSGIEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::SSGIPS, RootSignatureType::PostEffectGBuffer);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void SSGIEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
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
        std::clamp(m_intensity, 0.0f, 3.0f),
        std::max(0.5f, m_maxDistance),
        std::max(0.01f, m_thickness),
        std::clamp(m_stepScale, 0.25f, 2.0f));
    cb.params1 = Vector4(
        std::max(0.001f, camera->getNear()),
        std::max(camera->getNear() + 0.001f, camera->getFar()),
        static_cast<float>(std::clamp(m_maxSteps, 6, 48)),
        static_cast<float>(std::clamp(m_samples, 2, 16)));
    cb.params2 = Vector4(
        std::clamp(m_blendWeight, 0.0f, 1.0f),
        std::clamp(m_normalWeight, 0.0f, 1.0f),
        std::clamp(m_saturation, 0.0f, 2.0f),
        std::clamp(m_maxRadiance, 0.1f, 4.0f));
    cb.params3 = Vector4(
        static_cast<float>(std::clamp(m_debugMode, 0, 3)),
        std::clamp(m_debugScale, 0.1f, 8.0f),
        0.0f,
        0.0f);

    m_cb->update(cb);

    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex()));
    cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(GBufferRenderTargets::Instance().getSrvIndex(1)));

    drawFullscreenTriangle(cmd);
}

void SSGIEffect::inspectGUI()
{
    ImGui::SeparatorText("SSGI");
    ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 3.0f);
    ImGui::DragFloat("Max Distance", &m_maxDistance, 0.25f, 0.5f, 40.0f);
    ImGui::DragFloat("Thickness", &m_thickness, 0.005f, 0.01f, 1.0f);
    ImGui::SliderFloat("Step Scale", &m_stepScale, 0.25f, 2.0f);
    ImGui::SliderInt("Max Steps", &m_maxSteps, 6, 48);
    ImGui::SliderInt("Samples", &m_samples, 2, 16);
    ImGui::SliderFloat("Normal Weight", &m_normalWeight, 0.0f, 1.0f);
    ImGui::SliderFloat("Saturation", &m_saturation, 0.0f, 2.0f);
    ImGui::SliderFloat("Max Radiance", &m_maxRadiance, 0.1f, 4.0f);

    static const char* debugModes[] =
    {
        "Off",
        "Indirect Only",
        "Contribution Heat",
        "Final GI Mix"
    };
    ImGui::Combo("Debug View", &m_debugMode, debugModes, IM_ARRAYSIZE(debugModes));
    if (m_debugMode != 0)
    {
        ImGui::SliderFloat("Debug Scale", &m_debugScale, 0.1f, 8.0f);
    }
}
