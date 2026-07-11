#include "pch.h"
#include "Camera/CameraManager.h"
#include "Render/GBufferRenderTargets.h"
#include "System/TimeManager.h"
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
        TimeManager::Instance().getDeltaTime(),
        m_blendWeight);

    params.params1 = Vector4(
        1.0f / width,
        1.0f / height,
        0.0f,
        0.0f);

    params.graph = Vector4(
        std::max(0.0f, m_graphId),
        std::clamp(m_graphMetallic, 0.0f, 1.0f),
        std::clamp(m_graphRoughness, 0.0f, 1.0f),
        std::clamp(m_graphAo, 0.0f, 1.0f));
    params.graphBlend = Vector4(std::clamp(m_graphBlend, 0.0f, 1.0f), 0.0f, 0.0f, 0.0f);

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
    ImGui::DragFloat("Shutter Speed", &m_shutterSpeed, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Max Blur Radius", &m_maxBlurRadius, 0.1f, 0.0f, 32.0f);

    ImGui::SeparatorText("Shader Graph");
    ImGui::InputFloat("Graph ID", &m_graphId, 1.0f, 10.0f, "%.0f");
    if (m_graphId < 0.0f) m_graphId = 0.0f;
    ImGui::SliderFloat("Graph Metallic", &m_graphMetallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Roughness", &m_graphRoughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph AO", &m_graphAo, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Blend", &m_graphBlend, 0.0f, 1.0f);
}