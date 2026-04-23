#include "pch.h"
#include "ColorGradingEffect.h"

void ColorGradingEffect::initialize()
{
    registerPSO(ShaderID::ColorGradingPS);
    m_cb = std::make_unique<ConstantBuffer<CBuffer>>();
}

void ColorGradingEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    m_cb->update(m_params);

    applyPSO(cmd);

    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(
        1,
        DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex)
    );

    drawFullscreenTriangle(cmd);
}

void ColorGradingEffect::inspectGUI()
{
    ImGui::SeparatorText("Exposure / WhiteBalance");
    ImGui::SliderFloat("Exposure (EV)", &m_params.exposure, -5.0f, 5.0f);
    ImGui::SliderFloat("Temperature", &m_params.temperature, -1.0f, 1.0f);
    ImGui::SliderFloat("Tint", &m_params.tint, -1.0f, 1.0f);

    ImGui::SeparatorText("Color Adjustment");
    ImGui::SliderFloat("Contrast", &m_params.contrast, 0.0f, 2.0f);
    ImGui::SliderFloat("Saturation", &m_params.saturation, 0.0f, 2.0f);
    ImGui::SliderFloat("Hue Shift (deg)", &m_params.hueShift, -180.0f, 180.0f);

    ImGui::SeparatorText("Shadows / Midtones / Highlights");
    ImGui::ColorEdit3("Shadows", reinterpret_cast<float*>(&m_params.shadows));
    ImGui::ColorEdit3("Midtones", reinterpret_cast<float*>(&m_params.midtones));
    ImGui::ColorEdit3("Highlights", reinterpret_cast<float*>(&m_params.highlights));

    ImGui::SeparatorText("Tone Mapping");
    const char* modes[] = { "Linear", "ACES", "Filmic (Hable)" };
    ImGui::Combo("Mode", &m_params.tonemapMode, modes, 3);
}