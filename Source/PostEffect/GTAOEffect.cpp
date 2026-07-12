#include "pch.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Render/GBufferRenderTargets.h"
#include "GTAOEffect.h"

void GTAOEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::GTAOPS, RootSignatureType::PostEffectGBuffer);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void GTAOEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    auto* camera = CameraManager::Instance().getMainCamera();
    if (!cmd || !camera || inputSrvIndex == UINT_MAX)
    {
        return;
    }

    const float width = static_cast<float>(std::max(1, DX12::Instance().getScreenWidth()));
    const float height = static_cast<float>(std::max(1, DX12::Instance().getScreenHeight()));

    CBuffer cb{};
    cb.invProjection = camera->getProjection().Invert();
    cb.view = camera->getView();
    cb.params0 = Vector4(
        std::max(0.05f, m_radius),
        std::max(0.01f, m_thickness),
        std::clamp(m_intensity, 0.0f, 3.0f),
        std::clamp(m_power, 0.25f, 4.0f));
    cb.params1 = Vector4(
        1.0f / width,
        1.0f / height,
        std::max(0.001f, camera->getNear()),
        std::max(camera->getNear() + 0.001f, camera->getFar()));
    cb.params2 = Vector4(
        static_cast<float>(std::clamp(m_stepCount, 2, 12)),
        static_cast<float>(std::clamp(m_directionCount, 4, 16)),
        std::clamp(m_blendWeight, 0.0f, 1.0f),
        std::clamp(m_normalWeight, 0.0f, 1.0f));

    m_cb->update(cb);

    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex()));
    cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(GBufferRenderTargets::Instance().getSrvIndex(1)));

    drawFullscreenTriangle(cmd);
}

void GTAOEffect::inspectGUI()
{
    ImGui::SeparatorText("GTAO");
    ImGui::DragFloat("Radius", &m_radius, 0.01f, 0.05f, 4.0f);
    ImGui::DragFloat("Thickness", &m_thickness, 0.005f, 0.01f, 1.0f);
    ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 3.0f);
    ImGui::SliderFloat("Power", &m_power, 0.25f, 4.0f);
    ImGui::SliderInt("Steps", &m_stepCount, 2, 12);
    ImGui::SliderInt("Directions", &m_directionCount, 4, 16);
    ImGui::SliderFloat("Normal Weight", &m_normalWeight, 0.0f, 1.0f);
}