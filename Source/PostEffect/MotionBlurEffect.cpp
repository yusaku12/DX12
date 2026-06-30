#include "pch.h"
#include "MotionBlurEffect.h"
#include "Camera/CameraComponent.h"

void MotionBlurEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::MotionBlurPS, RootSignatureType::PostEffectDepth);
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
        TimeManager::Instance().getDeltaTime(),
        m_blendWeight);

    params.params1 = Vector4(
        camera->getNear(),
        camera->getFar(),
        1.0f / width,
        1.0f / height);

    m_cb->update(params);

    applyPSO(cmd);

    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(
        1,
        DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex)
    );
    cmd->SetGraphicsRootDescriptorTable(
        2,
        DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex())
    );

    drawFullscreenTriangle(cmd);

    m_prevViewProj = currentViewProj;
}

void MotionBlurEffect::inspectGUI()
{
    ImGui::SeparatorText("Motion Blur");
    ImGui::DragFloat("Shutter Speed", &m_shutterSpeed, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Max Blur Radius", &m_maxBlurRadius, 0.1f, 0.0f, 32.0f);
}