#include "pch.h"
#include "CanvasComponent.h"
#include "System\RuntimeUIManager.h"

void CanvasComponent::onEnable()
{
    LOG_INFO("CanvasComponent::onEnable() - Registering Canvas: %s", gameObject()->getName().c_str());
    RuntimeUIManager::Instance().registerCanvas(this);
}

void CanvasComponent::onDisable()
{
    RuntimeUIManager::Instance().unregisterCanvas(this);
}

void CanvasComponent::onDestroy()
{
    RuntimeUIManager::Instance().unregisterCanvas(this);
}

void CanvasComponent::inspectGUI()
{
    ImGui::DragInt("Sort Order", &m_sortOrder, 1.0f, -128, 128);
    ImGui::Checkbox("Receives Input", &m_receivesInput);

    const char* modes[] = { "Screen Overlay", "World Space" };
    int modeIdx = static_cast<int>(m_renderMode);
    if (ImGui::Combo("Render Mode", &modeIdx, modes, IM_ARRAYSIZE(modes)))
        m_renderMode = static_cast<CanvasRenderMode>(modeIdx);

    if (m_renderMode == CanvasRenderMode::WorldSpace)
    {
        float ws[2] = { m_worldSize.x, m_worldSize.y };
        if (ImGui::DragFloat2("World Size", ws, 1.f, 1.f, 4096.f))
            m_worldSize = Vector2(ws[0], ws[1]);
    }
}