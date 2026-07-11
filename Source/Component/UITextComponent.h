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

    float getGraphId() const { return m_graphId; }
    void setGraphId(float value) { m_graphId = std::max(0.0f, value); }

    float getGraphMetallic() const { return m_graphMetallic; }
    void setGraphMetallic(float value) { m_graphMetallic = std::clamp(value, 0.0f, 1.0f); }

    float getGraphRoughness() const { return m_graphRoughness; }
    void setGraphRoughness(float value) { m_graphRoughness = std::clamp(value, 0.0f, 1.0f); }

    float getGraphAo() const { return m_graphAo; }
    void setGraphAo(float value) { m_graphAo = std::clamp(value, 0.0f, 1.0f); }

    float getGraphBlend() const { return m_graphBlend; }
    void setGraphBlend(float value) { m_graphBlend = std::clamp(value, 0.0f, 1.0f); }

private:

    std::string m_text = "Text";
    Vector4 m_color = Vector4(0.97f, 0.97f, 0.99f, 1.0f);
    float m_fontScale = 1.0f;
    UITextAlignment m_alignment = UITextAlignment::MiddleCenter;
    float m_graphId = 0.0f;
    float m_graphMetallic = 0.0f;
    float m_graphRoughness = 1.0f;
    float m_graphAo = 1.0f;
    float m_graphBlend = 0.0f;
};