#pragma once

#include "Component.h"
#include "UI/UIAnimator.h"

//=====================================================
//! UI パネルコンポーネント
//!
//! 背景矩形（塗りつぶし色 + ボーダー）を描画するコンテナ。
//! 子ウィジェットのクリッピング領域としても機能する。
//=====================================================
class UIPanelComponent : public Component
{
public:

    void inspectGUI() override;

    const Vector4& getBackgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Vector4& v) { m_backgroundColor = v; }

    const Vector4& getBorderColor() const { return m_borderColor; }
    void setBorderColor(const Vector4& v) { m_borderColor = v; }

    float getBorderWidth() const { return m_borderWidth; }
    void setBorderWidth(float v) { m_borderWidth = std::max(0.f, v); }

    float getAlpha() const { return m_alpha; }
    void setAlpha(float v) { m_alpha = std::clamp(v, 0.f, 1.f); }

    //! UIAnimator を使ってフェードイン/フェードアウト
    void fadeIn (float duration = 0.3f, UIEaseType ease = UIEaseType::EaseOutQuad);
    void fadeOut(float duration = 0.3f, UIEaseType ease = UIEaseType::EaseOutQuad,
                 std::function<void()> onComplete = nullptr);

private:

    Vector4 m_backgroundColor = Vector4(0.05f, 0.05f, 0.08f, 0.88f);
    Vector4 m_borderColor     = Vector4(0.35f, 0.35f, 0.45f, 0.70f);
    float   m_borderWidth     = 1.0f;
    float   m_alpha           = 1.0f;
};
