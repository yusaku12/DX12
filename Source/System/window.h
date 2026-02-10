#pragma once

#include "Test\TestPolygon.h"

//=====================================================
// windowの管理を行うクラス
// その他のシステムを初期化する場所でもある
//=====================================================
class Window
{
public:

    explicit Window(HWND hwnd);
    ~Window();

    //! 更新処理
    void update();

    //! 描画処理
    void render();

    //! imgui描画処理
    void imguiRender();

    //! メッセージループ
    int run();

    //! ウィンドウプロシージャ
    LRESULT processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:

    //! タイトルバー更新
    void updateTitleBar();

    const HWND m_hwnd;
    DX12 m_dx12;
    std::unique_ptr<TestPolygon> m_testPolygon;
};