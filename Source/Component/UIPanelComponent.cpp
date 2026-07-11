#include "pch.h"
#include "UIPanelComponent.h"
#include "UI\UIAnimator.h"

void UIPanelComponent::inspectGUI()
{
    float bg[4] = { m_backgroundColor.x, m_backgroundColor.y,
                    m_backgroundColor.z, m_backgroundColor.w };
    if (ImGui::ColorEdit4("Background", bg))
        m_backgroundColor = Vector4(bg[0], bg[1], bg[2], bg[3]);

    float bc[4] = { m_borderColor.x, m_borderColor.y,
                    m_borderColor.z, m_borderColor.w };
    if (ImGui::ColorEdit4("Border Color", bc))
        m_borderColor = Vector4(bc[0], bc[1], bc[2], bc[3]);

    ImGui::DragFloat("Border Width", &m_borderWidth, 0.5f, 0.f, 32.f, "%.1f");
    ImGui::DragFloat("Alpha", &m_alpha, 0.01f, 0.f, 1.f, "%.2f");
}

void UIPanelComponent::fadeIn(float duration, UIEaseType ease)
{
    UIAnimator::Instance().animateFloat(&m_alpha, 1.f, duration, ease);
}

void UIPanelComponent::fadeOut(float duration, UIEaseType ease,
    std::function<void()> onComplete)
{
    UIAnimator::Instance().animateFloat(&m_alpha, 0.f, duration, ease,
        0.f, std::move(onComplete));
}