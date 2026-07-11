#include "pch.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "Render/DeferredRenderer.h"
#include "VolumetricFogEffect.h"

void VolumetricFogEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::VolumetricFogPS, RootSignatureType::PostEffectDepth);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void VolumetricFogEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    auto* camera = CameraManager::Instance().getMainCamera();
    if (!cmd || !camera || inputSrvIndex == UINT_MAX)
    {
        return;
    }

    CBuffer cb{};
    cb.projection = camera->getProjection();
    cb.invProjection = cb.projection.Invert();
    cb.invView = camera->getView().Invert();

    const Vector3 cameraPos = camera->getPosition();
    cb.cameraPos = Vector4(cameraPos.x, cameraPos.y, cameraPos.z, 0.0f);
    cb.cameraNearFar = Vector4(
        std::max(0.001f, camera->getNear()),
        std::max(camera->getNear() + 0.001f, camera->getFar()),
        0.0f,
        0.0f);

    const Vector3 lightDir = DeferredRenderer::Instance().getLightDirection();
    cb.lightDir = Vector4(lightDir.x, lightDir.y, lightDir.z, 0.0f);

    cb.fogColor = Vector4(m_fogColor.x, m_fogColor.y, m_fogColor.z, 0.0f);
    cb.params0 = Vector4(
        std::clamp(m_density, 0.0f, 1.0f),
        std::clamp(m_heightFalloff, 0.0f, 1.0f),
        std::max(1.0f, m_maxDistance),
        static_cast<float>(std::clamp(m_stepCount, 8, 96)));
    cb.params1 = Vector4(
        std::clamp(m_anisotropy, -0.8f, 0.8f),
        std::clamp(m_inscatter, 0.0f, 4.0f),
        std::clamp(m_ambient, 0.0f, 2.0f),
        std::clamp(m_atmoStrength, 0.0f, 2.0f));
    cb.params2 = Vector4(
        m_groundHeight,
        std::clamp(m_horizonBoost, 0.0f, 3.0f),
        std::clamp(m_depthFogBias, 0.0f, 1.0f),
        std::clamp(m_blendWeight, 0.0f, 1.0f));

    m_cb->update(cb);

    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex()));

    drawFullscreenTriangle(cmd);
}

void VolumetricFogEffect::inspectGUI()
{
    ImGui::SeparatorText("Volumetric Fog + Atmosphere");
    ImGui::ColorEdit3("Fog Color", &m_fogColor.x);
    ImGui::SliderFloat("Density", &m_density, 0.0f, 0.2f);
    ImGui::SliderFloat("Height Falloff", &m_heightFalloff, 0.0f, 0.2f);
    ImGui::DragFloat("Max Distance", &m_maxDistance, 1.0f, 1.0f, 500.0f);
    ImGui::SliderInt("Step Count", &m_stepCount, 8, 96);
    ImGui::SliderFloat("Anisotropy", &m_anisotropy, -0.8f, 0.8f);
    ImGui::SliderFloat("Inscatter", &m_inscatter, 0.0f, 4.0f);
    ImGui::SliderFloat("Ambient", &m_ambient, 0.0f, 2.0f);
    ImGui::SliderFloat("Atmosphere", &m_atmoStrength, 0.0f, 2.0f);
    ImGui::DragFloat("Ground Height", &m_groundHeight, 0.1f, -200.0f, 200.0f);
    ImGui::SliderFloat("Horizon Boost", &m_horizonBoost, 0.0f, 3.0f);
    ImGui::SliderFloat("Depth Fog Bias", &m_depthFogBias, 0.0f, 1.0f);
}
