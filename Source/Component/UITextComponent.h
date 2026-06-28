#pragma once

#include "Component.h"

enum class UITextAlignment : int
{
    TopLeft = 0,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

class UITextComponent : public Component
{
public:

    void awake() override;
    void inspectGUI() override;

    const std::string& getText() const { return m_text; }
    void setText(const std::string& value) { m_text = value; }

    const Vector4& getColor() const { return m_color; }
    void setColor(const Vector4& value) { m_color = value; }

    float getFontScale() const { return m_fontScale; }
    void setFontScale(float value) { m_fontScale = std::max(0.1f, value); }

    UITextAlignment getAlignment() const { return m_alignment; }
    void setAlignment(UITextAlignment value) { m_alignment = value; }

private:

    std::string m_text = "Text";
    Vector4 m_color = Vector4(0.97f, 0.97f, 0.99f, 1.0f);
    float m_fontScale = 1.0f;
    UITextAlignment m_alignment = UITextAlignment::MiddleCenter;
};