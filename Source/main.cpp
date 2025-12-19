#include "pch.h"
#include "System\Window.h"

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
INT WINAPI wWinMain(
    HINSTANCE instance,
    [[maybe_unused]] HINSTANCE prevInstance,
    [[maybe_unused]] LPWSTR cmdLine,
    INT cmdShow)
{
    //! サイズ調整
    DWORD dw_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    DWORD dw_ex_style = WS_EX_APPWINDOW;

    if (true) // resize
        dw_style |= WS_THICKFRAME;

    RECT rect{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    ::AdjustWindowRectEx(&rect, dw_style, FALSE, dw_ex_style);

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    //! Windowクラスの設定
    WNDCLASSEX wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = windowProc;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCEW(111));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = WINDOW_CLASS;

    if (!RegisterClassEx(&wcex))
        return -1;

    //! Window作成
    HWND hwnd = ::CreateWindowEx(
        dw_ex_style,
        WINDOW_CLASS,
        TITLE,
        dw_style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr,
        instance,
        nullptr);

    if (!hwnd)
        return -1;

    ShowWindow(hwnd, cmdShow);

    Window window(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&window));

    return window.run();
}