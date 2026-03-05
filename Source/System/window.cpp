#include "pch.h"
#include "Window.h"

Window::Window(HWND hwnd)
    : m_hwnd(hwnd)
    , m_dx12(hwnd)
{
    //! DescriptorHeapManager初期化
    DescriptorHeapManager::Instance().initialize();

    //! DX12初期化
    m_dx12.initialize();

    //! CommandListPool初期化（ワーカースレッド用）
    CommandListPool::Instance().initialize(m_dx12.getDevice(), 4);

    //! imgui初期化
    IMGUI_CTRL_INITIALIZE();

    //! シェーダーマネージャー初期化
    ShaderManager::Instance().initialize();

    //! PiplineState初期化
    PiplineState::Instance().initialize();

    //! RootSignatureManager初期化
    RootSignatureManager::Instance().initialize();

    //! TimeManager初期化
    TimeManager::Instance().initialize();

    //! CameraManager初期化
    CameraManager::Instance().initialize();

    //! オーディオマネージャー初期化
    AudioManager::Instance().initialize();

    //! シーンマネージャー初期化
    SceneManager::Instance().initialize();

    //! デバックプリミティブ描画初期化
    DebugPrimitive::Instance().initialize();

    //! 物理マネージャー初期化
    PhysicsWorld::Instance().initialize();

    //! 試しに読み込み
    //AudioManager::Instance().load("bgm", "Data/Audio/a.wav");
    //AudioManager::Instance().play("bgm", true);

    //AudioManager::Instance().setMasterVolume(0.05f);
}

Window::~Window()
{
    //! ImGui がある場合は先に終了処理
    IMGUI_CTRL_FINALIZE();

    //! オーディオマネージャー終了処理
    AudioManager::Instance().shutdown();

    //! デバックプリミティブ描画終了処理
    DebugPrimitive::Instance().shutdown();

    //! 物理マネージャー終了処理
    PhysicsWorld::Instance().shutdown();
}

void Window::update()
{
    //! TimeManager更新
    TimeManager::Instance().update();

    //! 物理シミュレーション更新
    float dt = TimeManager::Instance().getDeltaTime();
    PhysicsWorld::Instance().simulate(dt);
    PhysicsWorld::Instance().fetchResults();

    //! shaderManager更新
    ShaderManager::Instance().update();

    //! InputManager更新
    InputManager::Instance().update();

    //! SceneManager更新
    SceneManager::Instance().update();

    //! CameraManager更新
    CameraManager::Instance().update();

    //! ゲームオブジェクト更新
    GameObjectRegistry::Instance().update();

    //! オーディオマネージャー更新
    AudioManager::Instance().update(TimeManager::Instance().getDeltaTime());

    //! デバックプリミティブ更新
    DebugPrimitive::Instance().update(TimeManager::Instance().getDeltaTime());
}

void Window::render()
{
    //! フレーム開始処理
    TimeManager::Instance().frameStart(m_dx12.getGraphicsCommandList());

    //! 画面をクリア（メインコマンドリストにバリア・クリア・ビューポート・シザーを記録）
    m_dx12.screenClear();

    //! デバックプリミティブ描画（メインコマンドリストで実行）
    DebugPrimitive::Instance().render();

    //! シーンマネージャ描画（マルチスレッドでワーカーコマンドリストに記録）
    SceneManager::Instance().draw();

    //! メインコマンドリスト（pre-pass: バリア・クリア・デバッグ描画）を Close → Execute
    m_dx12.getGraphicsCommandList()->Close();
    ID3D12CommandList* preLists[] = { m_dx12.getGraphicsCommandList() };
    m_dx12.getCommandQueue()->ExecuteCommandLists(1, preLists);

    //! ワーカーコマンドリスト（描画コマンド）を Execute
    m_dx12.executeWorkerCommandLists();

    //! post-pass 用アロケータでメインコマンドリストをリセット（GPU Wait 不要）
    m_dx12.resetCommandListOnly();

    //! バックバッファをimgui用に準備
    m_dx12.prepareBackBufferForImGui();

    //! imgui描画
    imguiRender();

    //! 画面クリア後の後処理
    m_dx12.screenClearCleanup();

    //! TimeManagerフレーム終了処理
    TimeManager::Instance().frameEnd(m_dx12.getGraphicsCommandList());
}

void Window::imguiRender()
{
    //! imgui更新
    IMGUI_CTRL_UPDATE();

    //! ログ描画
    Logger::Instance().renderLog();

    //!TimeManagerのimgui描画
    TimeManager::Instance().imgui();

    //! SceneManagerのimgui描画
    SceneManager::Instance().debugOption();

    //! カメラマネージャーのimgui描画
    CameraManager::Instance().debugImgui();

    //! DX12のシーンimgui描画
    m_dx12.sceneImguiRender();

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
            updateTitleBar();
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
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            m_dx12.screenResize(width, height);
            IMGUI_CTRL_RESIZE(width, height);
        }
        break;

    case WM_MOUSEWHEEL:
    {
        InputManager::Instance().addMouseWheel(GET_WHEEL_DELTA_WPARAM(wparam));
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_ACTIVATE:
        InputManager::Instance().setWindowFocused(LOWORD(wparam) != WA_INACTIVE);
        break;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    return 0;
}

void Window::updateTitleBar()
{
    RECT rc{};
    GetClientRect(m_hwnd, &rc);

    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    wchar_t text[256];
    swprintf_s(text,
        L"DX12 | %dx%d | FPS: %.1f",
        width,
        height,
        static_cast<float>(TimeManager::Instance().getFPS()));

    SetWindowTextW(m_hwnd, text);
}