#include "pch.h"
#include "RectTransformComponent.h"

namespace
{
    bool dragVector2(const char* label, Vector2& value, float speed, float minValue, float maxValue)
    {
        float raw[2] = { value.x, value.y };
        if (!ImGui::DragFloat2(label, raw, speed, minValue, maxValue))
        {
            return false;
        }

        value = Vector2(raw[0], raw[1]);
        return true;
    }
}

ImRect RectTransformComponent::calculateRect(const ImRect& parentRect) const
{
    const ImVec2 parentSize = parentRect.GetSize();
    const ImVec2 anchorPoint(
        parentRect.Min.x + parentSize.x * m_anchor.x,
        parentRect.Min.y + parentSize.y * m_anchor.y);

    const ImVec2 rectSize(
        std::max(1.0f, m_size.x),
        std::max(1.0f, m_size.y));

    const ImVec2 rectMin(
        anchorPoint.x + m_position.x - rectSize.x * m_pivot.x,
        anchorPoint.y + m_position.y - rectSize.y * m_pivot.y);

    return ImRect(rectMin, ImVec2(rectMin.x + rectSize.x, rectMin.y + rectSize.y));
}

void RectTransformComponent::inspectGUI()
{
    dragVector2("Anchor", m_anchor, 0.01f, 0.0f, 1.0f);
    dragVector2("Position", m_position, 1.0f, -4096.0f, 4096.0f);
    dragVector2("Size", m_size, 1.0f, 1.0f, 4096.0f);
    dragVector2("Pivot", m_pivot, 0.01f, 0.0f, 1.0f);
}