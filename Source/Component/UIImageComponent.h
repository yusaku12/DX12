#pragma once

#include "Component.h"
#include "UI/UIAnimator.h"

//=====================================================
//! UI 画像コンポーネント
//!
//! テクスチャ付き矩形を描画する。
//! テクスチャパスを設定するとランタイムロードする。
//=====================================================
class UIImageComponent : public Component
{
public:

    void awake()   override;
    void inspectGUI() override;

    //! テクスチャを設定（パスが変わると自動再ロード）
    void setTexturePath(const std::wstring& path);
    const std::wstring& getTexturePath() const { return m_texturePath; }

    const Vector4& getTintColor() const { return m_tintColor; }
    void setTintColor(const Vector4& v) { m_tintColor = v; }

    float getAlpha() const { return m_alpha; }
    void setAlpha(float v) { m_alpha = std::clamp(v, 0.f, 1.f); }

    //! UIAnimator を使ってフェードイン/フェードアウト
    void fadeIn (float duration = 0.3f, UIEaseType ease = UIEaseType::EaseOutQuad);
    void fadeOut(float duration = 0.3f, UIEaseType ease = UIEaseType::EaseOutQuad,
                 std::function<void()> onComplete = nullptr);

    //! 現在ロード済みのテクスチャ SRV インデックス（UINT_MAX = 未ロード）
    UINT getSrvIndex() const;

private:

    std::wstring m_texturePath;
    Vector4      m_tintColor = Vector4(1, 1, 1, 1);
    float        m_alpha     = 1.0f;

    class LoadTexture* m_texture = nullptr; //!< TextureManager 所有、解放不要
};
