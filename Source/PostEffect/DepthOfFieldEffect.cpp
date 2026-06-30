#include "pch.h"
#include "DepthOfFieldEffect.h"
#include "Camera/CameraComponent.h"

void DepthOfFieldEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::DepthOfFieldPS, RootSignatureType::PostEffectDepth);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void DepthOfFieldEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    const float width = static_cast<float>(DX12::Instance().getScreenWidth());
    const float height = static_cast<float>(DX12::Instance().getScreenHeight());
    m_params.texelSize = Vector2(1.0f / width, 1.0f / height);

    if (auto* camera = CameraManager::Instance().getMainCamera())
    {
        m_params.nearZ = camera->getNear();
        m_params.farZ = camera->getFar();
    }

    m_params.blendWeight = m_blendWeight;

    m_cb->update(m_params);

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
}

void DepthOfFieldEffect::inspectGUI()
{
    ImGui::SeparatorText("Focus");
    ImGui::DragFloat("Focus Distance", &m_params.focusDistance, 0.05f, 0.01f, 1000.0f);
    ImGui::DragFloat("Focus Range", &m_params.focusRange, 0.05f, 0.01f, 1000.0f);

    ImGui::SeparatorText("Bokeh");
    ImGui::DragFloat("Aperture", &m_params.aperture, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Max Blur Radius", &m_params.maxBlurRadius, 0.1f, 0.0f, 32.0f);
}