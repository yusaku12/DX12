#include "pch.h"
#include "Camera/CameraManager.h"
#include "Render/GBufferRenderTargets.h"
#include "MotionBlurEffect.h"
#include "Camera/CameraComponent.h"

void MotionBlurEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::MotionBlurPS, RootSignatureType::PostEffectVelocity);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void MotionBlurEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    auto* camera = CameraManager::Instance().getMainCamera();
    if (!camera)
        return;

    Matrix view = camera->getView();
    Matrix proj = camera->getProjection();
    Matrix currentViewProj = view * proj;

    if (!m_hasPrev)
    {
        m_prevViewProj = currentViewProj;
        m_hasPrev = true;
    }

    CBuffer params{};
    params.currentViewProj = currentViewProj;
    params.prevViewProj = m_prevViewProj;
    params.invViewProj = currentViewProj.Invert();

    float width = static_cast<float>(DX12::Instance().getScreenWidth());
    float height = static_cast<float>(DX12::Instance().getScreenHeight());

    params.params0 = Vector4(
        m_shutterSpeed,
        m_maxBlurRadius,
        m_blendWeight,
        std::clamp(m_velocityReject, 0.0f, 1.0f));

    params.params1 = Vector4(
        1.0f / width,
        1.0f / height,
        static_cast<float>(std::clamp(m_minSamples, 2, 16)),
        static_cast<float>(std::clamp(m_maxSamples, std::max(m_minSamples, 2), 16)));

    m_cb->update(params);

    applyPSO(cmd);

    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(
        1,
        DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex)
    );
    cmd->SetGraphicsRootDescriptorTable(
        2,
        DescriptorHeapManager::Instance().getGPUHandle(GBufferRenderTargets::Instance().getSrvIndex(3))
    );

    drawFullscreenTriangle(cmd);

    m_prevViewProj = currentViewProj;
}

void MotionBlurEffect::inspectGUI()
{
    ImGui::SeparatorText("Motion Blur");
    ImGui::DragFloat("Shutter Scale", &m_shutterSpeed, 0.01f, 0.0f, 2.5f);
    ImGui::DragFloat("Max Blur Radius (px)", &m_maxBlurRadius, 0.1f, 0.0f, 48.0f);
    ImGui::SliderFloat("Velocity Reject", &m_velocityReject, 0.0f, 1.0f);
    ImGui::SliderInt("Min Samples", &m_minSamples, 2, 12);
    ImGui::SliderInt("Max Samples", &m_maxSamples, 4, 16);
    if (m_maxSamples < m_minSamples)
    {
        m_maxSamples = m_minSamples;
    }
}