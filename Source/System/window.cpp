#include "pch.h"
#include "Window.h"

Window::Window(HWND hwnd)
    : m_hwnd(hwnd)
    , m_dx12(hwnd)
{
    //! imgui初期化
    IMGUI_CTRL_INITIALIZE();

    //! TimeManager初期化
    TimeManager::Instance().initialize();

    //! PiplineState初期化
    PiplineState::Instance().initialize();

    //! DescriptorHeapManager初期化
    DescriptorHeapManager::Instance().initialize();

    //! CameraManager初期化
    CameraManager::Instance().initialize();

    //! PMXモデル読み込み
    m_pmxActor = std::make_unique<PmxActor>(L"Data/Model/Kazusa_ByPOWER_v1.0/Kazusa_ByPOWER.pmx");
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
    //! TimeManager更新
    TimeManager::Instance().update();

    //! imgui更新
    IMGUI_CTRL_UPDATE();

    //! SceneManager更新
    SceneManager::Instance().update();

    //! CameraManager更新
    CameraManager::Instance().update();

    //! ゲームオブジェクト更新
    GameObjectRegistry::Instance().update();
}

void Window::render()
{
    //! フレーム開始処理
    TimeManager::Instance().frameStart(m_dx12.getGraphicsCommandList());

    //! imgui描画
    imguiRender();

    //! 画面をクリア
    m_dx12.screenClear();

    //! PMXモデル描画
    m_pmxActor->render();

    //! シーンマネージャ描画
    SceneManager::Instance().draw();

    //! imguiの描画情報を設定
    IMGUI_CTRL_RENDER_INFO();

    //! TimeManagerフレーム終了処理
    TimeManager::Instance().frameEnd(m_dx12.getGraphicsCommandList());

    //! レンダーターゲットを元に戻し、コマンド終了
    m_dx12.renderTargetUndo();

    //! プラットフォームを追加してウィンドウ更新
    IMGUI_CTRL_UPDATE_RENDER();

    //! 画面クリア後の後処理
    m_dx12.screenClearCleanup();
}

void Window::imguiRender()
{
    //! ログ描画
    Logger::Instance().renderLog();

    //!TimeManagerのimgui描画
    TimeManager::Instance().imgui();

    //! SceneManagerのimgui描画
    SceneManager::Instance().debugOption();

    //! EditorManagerのimgui描画
    EditorManager::Instance().imgui();

    //! imgui描画
    IMGUI_CTRL_RENDER();
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
            Logger::Instance().logCall(LogLevel::INFO, "resize");
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