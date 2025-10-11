#pragma once

#include "Graphics\polygon.h"

//=====================================================
// Window クラス
//=====================================================
class Window
{
public:

    Window(HWND hwnd);
    ~Window();

    //! 更新処理
    void update();

    //! 描画処理
    void render();

    //! メッセージループ
    int run();

    //! ウィンドウプロシージャ
    LRESULT processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:

    const HWND m_hwnd;
    DX12 m_dx12;
    std::unique_ptr<Polygon> m_polygon;
};