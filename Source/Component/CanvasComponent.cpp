#include "pch.h"
#include "CanvasComponent.h"
#include "System\RuntimeUIManager.h"

void CanvasComponent::onEnable()
{
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
}