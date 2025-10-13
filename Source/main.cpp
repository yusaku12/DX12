#include "pch.h"
#include "System\window.h"

static constexpr LONG SCREEN_WIDTH = static_cast<LONG>(1280);
static constexpr LONG SCREEN_HEIGHT = static_cast<LONG>(720);
static constexpr LPCWSTR TITLE = L"DX12";
static constexpr LPCWSTR WINDOW_CLASS = L"DX12Class";

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Window* w = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return w ? w->processMessage(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
}

//=====================================================
// エントリーポイント
//=====================================================
INT WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, LPWSTR cmdLine, INT cmdShow)
{
    //! サイズ調整
    DWORD dw_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    DWORD dw_ex_style = WS_EX_APPWINDOW;
    RECT rect;
    bool resize = true;
    if (resize)
    {
        dw_style |= WS_THICKFRAME;
    }
    ::SetRect(&rect, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    ::AdjustWindowRectEx(&rect, dw_style, FALSE, 0);
    LONG width = rect.right - rect.left;
    LONG height = rect.bottom - rect.top;

    //! Windowクラスの設定
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = windowProc;		             //!< ウィンドウのメッセージを処理するためのコールバック関数
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIcon(instance, (LPCWSTR)111);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); //!< ウィンドウ背景色
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = WINDOW_CLASS;
    wcex.hIconSm = 0;
    RegisterClassEx(&wcex);

    //! Window作成
    HWND hwnd = ::CreateWindowEx(dw_ex_style, WINDOW_CLASS, TITLE, dw_style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, nullptr, instance, NULL);

    ShowWindow(hwnd, cmdShow);
    Window window(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&window));
    return window.run();
}