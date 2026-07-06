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
    ImGui::DragFloat("Alpha",        &m_alpha,       0.01f, 0.f, 1.f,  "%.2f");

    ImGui::SeparatorText("Shader Graph");
    ImGui::InputFloat("Graph ID", &m_graphId, 1.0f, 10.0f, "%.0f");
    m_graphId = std::max(0.0f, m_graphId);
    ImGui::SliderFloat("Graph Metallic", &m_graphMetallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Roughness", &m_graphRoughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph AO", &m_graphAo, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Blend", &m_graphBlend, 0.0f, 1.0f);
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
