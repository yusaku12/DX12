#pragma once

#include "Component.h"

//=====================================================
//! Canvas の描画モード
//=====================================================
enum class CanvasRenderMode : int
{
    ScreenOverlay,  //!< スクリーン座標 2D オーバーレイ（デフォルト）
    WorldSpace,     //!< ワールド空間に配置（TransformComponent のワールド行列を使用）
};

//=====================================================
//! Canvas コンポーネント
//!
//! 配下の Widget を UI 履歴に登録し、RuntimeUIManager が
//! この Canvas 単位で描画・入力処理を行う。
//=====================================================
class CanvasComponent : public Component
{
public:

    void onEnable()  override;
    void onDisable() override;
    void onDestroy() override;
    void inspectGUI() override;

    int  getSortOrder() const { return m_sortOrder; }
    void setSortOrder(int value) { m_sortOrder = value; }

    bool receivesInput() const { return m_receivesInput; }
    void setReceivesInput(bool value) { m_receivesInput = value; }

    CanvasRenderMode getRenderMode() const { return m_renderMode; }
    void setRenderMode(CanvasRenderMode mode) { m_renderMode = mode; }

    //! WorldSpace 時の Canvas サイズ（ピクセル単位）
    const Vector2& getWorldSize() const { return m_worldSize; }
    void setWorldSize(const Vector2& v) { m_worldSize = v; }

private:

    int              m_sortOrder    = 0;
    bool             m_receivesInput = true;
    CanvasRenderMode m_renderMode   = CanvasRenderMode::ScreenOverlay;
    Vector2          m_worldSize    = Vector2(800.f, 600.f); //!< WorldSpace 時のローカルサイズ
};