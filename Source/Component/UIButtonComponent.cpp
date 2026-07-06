#include "pch.h"
#include "UIButtonComponent.h"
#include "GameObject/GameObject.h"
#include "RectTransformComponent.h"
#include "System\EventBus.h"

void UIButtonComponent::awake()
{
    if (gameObject() && !gameObject()->getComponent<RectTransformComponent>())
    {
        gameObject()->addComponent<RectTransformComponent>();
    }
}

void UIButtonComponent::inspectGUI()
{
    std::array<char, 256> labelBuffer{};
    strncpy_s(labelBuffer.data(), labelBuffer.size(), m_label.c_str(), _TRUNCATE);
    if (ImGui::InputText("Label", labelBuffer.data(), labelBuffer.size()))
    {
        m_label = labelBuffer.data();
    }

    std::array<char, 256> eventBuffer{};
    strncpy_s(eventBuffer.data(), eventBuffer.size(), m_clickEventName.c_str(), _TRUNCATE);
    if (ImGui::InputText("Click Event", eventBuffer.data(), eventBuffer.size()))
    {
        m_clickEventName = eventBuffer.data();
    }

    float normal[4] = { m_normalColor.x, m_normalColor.y, m_normalColor.z, m_normalColor.w };
    if (ImGui::ColorEdit4("Normal Color", normal))
    {
        m_normalColor = Vector4(normal[0], normal[1], normal[2], normal[3]);
    }

    float hover[4] = { m_hoverColor.x, m_hoverColor.y, m_hoverColor.z, m_hoverColor.w };
    if (ImGui::ColorEdit4("Hover Color", hover))
    {
        m_hoverColor = Vector4(hover[0], hover[1], hover[2], hover[3]);
    }

    float pressed[4] = { m_pressedColor.x, m_pressedColor.y, m_pressedColor.z, m_pressedColor.w };
    if (ImGui::ColorEdit4("Pressed Color", pressed))
    {
        m_pressedColor = Vector4(pressed[0], pressed[1], pressed[2], pressed[3]);
    }

    float textColor[4] = { m_textColor.x, m_textColor.y, m_textColor.z, m_textColor.w };
    if (ImGui::ColorEdit4("Text Color", textColor))
    {
        m_textColor = Vector4(textColor[0], textColor[1], textColor[2], textColor[3]);
    }

    ImGui::DragFloat("Font Scale", &m_fontScale, 0.01f, 0.1f, 4.0f, "%.2f");
    ImGui::DragFloat("Corner Rounding", &m_cornerRounding, 0.5f, 0.0f, 64.0f, "%.1f");

    ImGui::SeparatorText("Shader Graph");
    ImGui::InputFloat("Graph ID", &m_graphId, 1.0f, 10.0f, "%.0f");
    m_graphId = std::max(0.0f, m_graphId);
    ImGui::SliderFloat("Graph Metallic", &m_graphMetallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Roughness", &m_graphRoughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph AO", &m_graphAo, 0.0f, 1.0f);
    ImGui::SliderFloat("Graph Blend", &m_graphBlend, 0.0f, 1.0f);

    ImGui::Checkbox("Interactable", &m_interactable);
    ImGui::Checkbox("Block Mouse Input", &m_blockMouseInput);
}

bool UIButtonComponent::invokeClick() const
{
    if (!isInteractable() || !isActiveInHierarchy())
    {
        return false;
    }

    UIButtonClickEvent payload{};
    payload.buttonObjectId = gameObject() ? gameObject()->getInstanceId() : 0;
    payload.buttonObjectName = gameObject() ? gameObject()->getName() : std::string();
    payload.eventName = m_clickEventName;

    EventBus::Instance().publish("UI.Button.Click", payload, payload.buttonObjectId);
    if (!m_clickEventName.empty())
    {
        EventBus::Instance().publish(m_clickEventName, payload, payload.buttonObjectId);
    }

    return true;
}