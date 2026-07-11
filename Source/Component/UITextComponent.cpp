#include "pch.h"
#include "UITextComponent.h"
#include "GameObject/GameObject.h"
#include "RectTransformComponent.h"

namespace
{
    constexpr const char* kAlignmentLabels[] =
    {
        "Top Left",
        "Top Center",
        "Top Right",
        "Middle Left",
        "Middle Center",
        "Middle Right",
        "Bottom Left",
        "Bottom Center",
        "Bottom Right",
    };
}

void UITextComponent::awake()
{
    if (gameObject() && !gameObject()->getComponent<RectTransformComponent>())
    {
        gameObject()->addComponent<RectTransformComponent>();
    }
}

void UITextComponent::inspectGUI()
{
    std::array<char, 512> buffer{};
    strncpy_s(buffer.data(), buffer.size(), m_text.c_str(), _TRUNCATE);
    if (ImGui::InputTextMultiline("Text", buffer.data(), buffer.size(), ImVec2(-1.0f, 80.0f)))
    {
        m_text = buffer.data();
    }

    float color[4] = { m_color.x, m_color.y, m_color.z, m_color.w };
    if (ImGui::ColorEdit4("Color", color))
    {
        m_color = Vector4(color[0], color[1], color[2], color[3]);
    }

    ImGui::DragFloat("Font Scale", &m_fontScale, 0.01f, 0.1f, 4.0f, "%.2f");

    int alignment = static_cast<int>(m_alignment);
    if (ImGui::Combo("Alignment", &alignment, kAlignmentLabels, IM_ARRAYSIZE(kAlignmentLabels)))
    {
        m_alignment = static_cast<UITextAlignment>(alignment);
    }

    ImGui::SeparatorText("Shader Graph");
    ImGui::InputFloat("Graph ID", &m_graphId, 1.0f, 10.0f, "%.0f");
    m_graphId = std::max(0.0f, m_graphId);
    ImGui::SliderFloat("Graph Metallic", &m_graphMetallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Roughness", &m_graphRoughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph AO", &m_graphAo, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Blend", &m_graphBlend, 0.0f, 1.0f);
}