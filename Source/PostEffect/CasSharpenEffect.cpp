#include "pch.h"
#include "CasSharpenEffect.h"

void CasSharpenEffect::initialize()
{
    registerPSO(ShaderID::CasSharpenPS);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
}

void CasSharpenEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    const float width = static_cast<float>(DX12::Instance().getScreenWidth());
    const float height = static_cast<float>(DX12::Instance().getScreenHeight());

    CBuffer cb{};
    cb.params0 = Vector4(
        std::clamp(m_strength, 0.0f, 1.0f),
        std::clamp(m_clampAmount, 0.0f, 1.0f),
        width > 0.0f ? 1.0f / width : 0.0f,
        height > 0.0f ? 1.0f / height : 0.0f);
    m_cb->update(cb);

    applyPSO(cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    drawFullscreenTriangle(cmd);
}

void CasSharpenEffect::inspectGUI()
{
    ImGui::SeparatorText("FSR RCAS");
    ImGui::SliderFloat("Strength", &m_strength, 0.0f, 1.0f);
    ImGui::SliderFloat("Clamp", &m_clampAmount, 0.0f, 1.0f);
}
