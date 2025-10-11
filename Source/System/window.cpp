#include "pch.h"
#include "window.h"

Window::Window(HWND hwnd)
    : m_hwnd(hwnd)
    , m_dx12(hwnd)
{
    //! imgui初期化
    IMGUI_CTRL_INITIALIZE();

    //! ポリゴンクラス初期化
    m_polygon = std::make_unique<Polygon>();
}

Window::~Window()
{
    //! GPU 完了待ち
    m_dx12.safeGPUWait();

    //! ImGui がある場合は先に終了処理
    IMGUI_CTRL_FINALIZE();
}

void Window::update()
{
    //! imgui更新
    IMGUI_CTRL_UPDATE();
}

void Window::render()
{
    //! ログ描画
    Logger::getInstance().renderLog();

    //! imgui描画
    IMGUI_CTRL_RENDER();

    //! 画面をクリア
    m_dx12.screenClear();

    //! ポリゴン描画
    m_polygon->render();

    //! imguiの描画情報を設定
    IMGUI_CTRL_RENDER_INFO();

    //! レンダーターゲットを元に戻し、コマンド終了
    m_dx12.renderTargetUndo();

    //! プラットフォームを追加してウィンドウ更新
    IMGUI_CTRL_UPDATE_RENDER();

    //! 画面クリア後の後処理
    m_dx12.screenClearCleanup();
}

int Window::run()
{
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            //! 更新、描画
            update();
            render();
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT Window::processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    //! ImGui にメッセージを渡す
    IMGUI_CTRL_WND_PRC_HANDLER(hwnd, msg, wparam, lparam);

    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED)
        {
            Logger::getInstance().logCall(LogLevel::INFO, "resize");
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            m_dx12.screenResize(width, height);
            IMGUI_CTRL_RESIZE(width, height);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    return 0;
}