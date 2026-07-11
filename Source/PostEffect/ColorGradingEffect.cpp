#include "pch.h"
#include "ColorGradingEffect.h"

void ColorGradingEffect::initialize()
{
    registerPSO(ShaderID::ColorGradingPS);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
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
    ImGui::SeparatorText("ACES + Auto Exposure");
    bool autoExposure = m_params.autoExposureEnabled > 0.5f;
    if (ImGui::Checkbox("Auto Exposure", &autoExposure))
    {
        m_params.autoExposureEnabled = autoExposure ? 1.0f : 0.0f;
    }

    ImGui::SliderFloat("Manual Exposure (EV)", &m_params.exposure, -8.0f, 8.0f);
    ImGui::SliderFloat("Exposure Compensation (EV)", &m_params.exposureCompensation, -5.0f, 5.0f);
    ImGui::SliderFloat("Middle Gray", &m_params.middleGray, 0.08f, 0.36f);
    ImGui::SliderFloat("Auto EV Min", &m_params.minEV, -12.0f, 0.0f);
    ImGui::SliderFloat("Auto EV Max", &m_params.maxEV, 0.0f, 12.0f);
    ImGui::SliderFloat("Auto Exposure Strength", &m_params.autoExposureStrength, 0.0f, 2.0f);

    if (m_params.minEV > m_params.maxEV)
    {
        std::swap(m_params.minEV, m_params.maxEV);
    }

    ImGui::SeparatorText("WhiteBalance");
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
    ImGui::TextUnformatted("Unified ACES is always applied in this effect.");
}