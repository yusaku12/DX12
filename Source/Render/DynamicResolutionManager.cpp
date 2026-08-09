#include "pch.h"
#include "Graphics/DX12.h"
#include "Render/FidelityFXUpscaler.h"
#include "System/TimeManager.h"
#include "DynamicResolutionManager.h"

void DynamicResolutionManager::update()
{
    if (!m_enabled || !m_autoMode)
    {
        return;
    }

    if (m_cooldown > 0)
    {
        --m_cooldown;
        return;
    }

    const float fps = static_cast<float>(TimeManager::Instance().getFPS());
    if (fps <= 1.0f)
    {
        return;
    }

    float nextScale = m_scale;
    if (fps < (m_targetFps - 2.0f))
    {
        nextScale -= m_stepDown;
    }
    else if (fps > (m_targetFps + 6.0f))
    {
        nextScale += m_stepUp;
    }

    nextScale = std::clamp(nextScale, m_minScale, m_maxScale);
    if (std::abs(nextScale - m_scale) < 0.005f)
    {
        return;
    }

    applyScale(nextScale);
    m_cooldown = std::max(1, m_cooldownFrames);
}

void DynamicResolutionManager::renderDebugContents()
{
    FidelityFXUpscaler::Instance().renderDebugContents();

    ImGui::SeparatorText("Dynamic Resolution");

    if (ImGui::Checkbox("Enable Dynamic Resolution", &m_enabled) && !m_enabled)
    {
        m_autoMode = false;
        applyScale(1.0f);
    }

    if (!m_enabled)
    {
        return;
    }

    ImGui::Checkbox("Auto Scale", &m_autoMode);
    ImGui::SliderFloat("Scale", &m_scale, 0.50f, 1.00f, "%.2f");
    ImGui::SliderFloat("Min Scale", &m_minScale, 0.50f, 1.00f, "%.2f");
    ImGui::SliderFloat("Max Scale", &m_maxScale, 0.50f, 1.00f, "%.2f");
    if (m_maxScale < m_minScale)
    {
        m_maxScale = m_minScale;
    }

    ImGui::SliderFloat("Target FPS", &m_targetFps, 30.0f, 240.0f, "%.0f");
    ImGui::SliderFloat("Step Down", &m_stepDown, 0.005f, 0.10f, "%.3f");
    ImGui::SliderFloat("Step Up", &m_stepUp, 0.005f, 0.10f, "%.3f");
    ImGui::SliderInt("Cooldown Frames", &m_cooldownFrames, 1, 120);

    if (!m_autoMode)
    {
        if (ImGui::Button("Apply Scale"))
        {
            applyScale(m_scale);
        }
    }

    ImGui::Text("Render Size: %d x %d", DX12::Instance().getScreenWidth(), DX12::Instance().getScreenHeight());
}

void DynamicResolutionManager::applyScale(float scale)
{
    const float clamped = std::clamp(scale, m_minScale, m_maxScale);
    m_scale = clamped;
    DX12::Instance().setRenderScale(clamped);
}