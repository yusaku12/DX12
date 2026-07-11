#pragma once

#include "Component.h"

class RectTransformComponent : public Component
{
public:

    ImRect calculateRect(const ImRect& parentRect) const;
    void inspectGUI() override;

    const Vector2& getAnchor() const { return m_anchor; }
    void setAnchor(const Vector2& value) { m_anchor = value; }

    const Vector2& getPosition() const { return m_position; }
    void setPosition(const Vector2& value) { m_position = value; }

    const Vector2& getSize() const { return m_size; }
    void setSize(const Vector2& value) { m_size = value; }

    const Vector2& getPivot() const { return m_pivot; }
    void setPivot(const Vector2& value) { m_pivot = value; }

private:

    Vector2 m_anchor = Vector2(0.5f, 0.5f);
    Vector2 m_position = Vector2::Zero;
    Vector2 m_size = Vector2(220.0f, 56.0f);
    Vector2 m_pivot = Vector2(0.5f, 0.5f);
};