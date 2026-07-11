#pragma once

#include "Component.h"

struct UIButtonClickEvent
{
    uint64_t buttonObjectId = 0;
    std::string buttonObjectName;
    std::string eventName;
};

class UIButtonComponent : public Component
{
public:

    void awake() override;
    void inspectGUI() override;

    bool isInteractable() const { return m_interactable; }
    void setInteractable(bool value) { m_interactable = value; }

    bool blocksMouseInput() const { return m_blockMouseInput; }
    void setBlockMouseInput(bool value) { m_blockMouseInput = value; }

    const std::string& getLabel() const { return m_label; }
    void setLabel(const std::string& value) { m_label = value; }

    const std::string& getClickEventName() const { return m_clickEventName; }
    void setClickEventName(const std::string& value) { m_clickEventName = value; }

    const Vector4& getNormalColor() const { return m_normalColor; }
    void setNormalColor(const Vector4& value) { m_normalColor = value; }

    const Vector4& getHoverColor() const { return m_hoverColor; }
    void setHoverColor(const Vector4& value) { m_hoverColor = value; }

    const Vector4& getPressedColor() const { return m_pressedColor; }
    void setPressedColor(const Vector4& value) { m_pressedColor = value; }

    const Vector4& getTextColor() const { return m_textColor; }
    void setTextColor(const Vector4& value) { m_textColor = value; }

    float getFontScale() const { return m_fontScale; }
    void setFontScale(float value) { m_fontScale = std::max(0.1f, value); }

    float getCornerRounding() const { return m_cornerRounding; }
    void setCornerRounding(float value) { m_cornerRounding = std::max(0.0f, value); }

    bool invokeClick() const;

private:

    std::string m_label = "Button";
    std::string m_clickEventName;
    Vector4 m_normalColor = Vector4(0.14f, 0.22f, 0.38f, 0.92f);
    Vector4 m_hoverColor = Vector4(0.19f, 0.31f, 0.52f, 0.96f);
    Vector4 m_pressedColor = Vector4(0.08f, 0.16f, 0.28f, 0.98f);
    Vector4 m_textColor = Vector4(0.98f, 0.98f, 0.99f, 1.0f);
    float m_fontScale = 1.0f;
    float m_cornerRounding = 14.0f;
    bool m_interactable = true;
    bool m_blockMouseInput = true;
};