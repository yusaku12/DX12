#include "pch.h"
#include "Window.h"

Window::Window(HWND hwnd)
    : m_hwnd(hwnd)
    , m_dx12(hwnd)
{
    // DescriptorHeapManager初期化
    DescriptorHeapManager::Instance().initialize();

    // DX12初期化
    m_dx12.initialize();

    // GBufferRenderTargets初期化
    GBufferRenderTargets::Instance().initialize();

    // CommandListPool初期化
    CommandListPool::Instance().initialize(m_dx12.getDevice(), m_dx12.getFence(), 4);

    // imgui初期化
    IMGUI_CTRL_INITIALIZE();

    // シェーダーマネージャー初期化
    ShaderManager::Instance().initialize();

    // PiplineState初期化
    PiplineState::Instance().initialize();

    // RootSignatureManager初期化
    RootSignatureManager::Instance().initialize();

    // IBL 初期化
    IBLManager::Instance().initialize();

    // ポストエフェクト用ピンポンRT初期化
    PostEffectRenderTargets::Instance().initialize();

    // Deferred Renderer 初期化
    DeferredRenderer::Instance().initialize();

    // シャドウマップレンダラー初期化
    ShadowMapRenderer::Instance().initialize();

    // TimeManager初期化
    TimeManager::Instance().initialize();

    // CameraManager初期化
    CameraManager::Instance().initialize();

    // オーディオマネージャー初期化
    AudioManager::Instance().initialize();

    // デバックプリミティブ描画初期化
    DebugPrimitive::Instance().initialize();

    // RenderPipeline初期化
    RenderPipeline::Instance().initialize();

    // 物理マネージャー初期化
    PhysicsWorld::Instance().initialize();

    // シーンマネージャー初期化
    SceneManager::Instance().initialize();
}

Window::~Window()
{
    // GPU 完了待ち
    m_dx12.safeGPUWait();

    // シーン終了処理
    SceneManager::Instance().shutdown();

    // GameObject 破棄
    GameObjectRegistry::Instance().shutdown();

    // 描画終了処理
    RenderManager::Instance().shutdown();

    // CameraManager終了処理
    CameraManager::Instance().shutdown();

    // ImGui がある場合は先に終了処理
    IMGUI_CTRL_FINALIZE();

    // オーディオマネージャー終了処理
    AudioManager::Instance().shutdown();

    // デバックプリミティブ描画終了処理
    DebugPrimitive::Instance().shutdown();

    // 物理マネージャー終了処理
    PhysicsWorld::Instance().shutdown();
}

void Window::update()
{
    // TimeManager更新
    TimeManager::Instance().update();

    // shaderManager更新
    ShaderManager::Instance().update();

    // InputManager更新
    InputManager::Instance().update();

    // デバックプリミティブ フレーム開始（前フレームの描画リクエストをクリア）
    DebugPrimitive::Instance().beginFrame();

    // SceneManager更新
    SceneManager::Instance().update();

    // ゲームオブジェクト更新（FreeCameraComponent など、カメラ位置を動かすコンポーネントを先に更新）
    GameObjectRegistry::Instance().update();

    // CameraManager更新（ゲームオブジェクト更新後に GPU バッファを確定させる）
    CameraManager::Instance().update();

    // エディタ更新（オブジェクトピッキング等、カメラ確定後に実行）
    EditorManager::Instance().update();

    // 物理シミュレーション更新
    float dt = TimeManager::Instance().getDeltaTime();
    PhysicsWorld::Instance().simulate(dt);

    // オーディオマネージャー更新
    AudioManager::Instance().update(dt);
}

void Window::render()
{
    // フレーム開始処理
    TimeManager::Instance().frameStart(m_dx12.getGraphicsCommandList());

    // 画面をクリア（描画パスに応じて切替）
    m_dx12.screenClear(CameraManager::Instance().getMainRenderPath());

    // シーンマネージャ描画（GBuffer パスをワーカーコマンドに記録）
    SceneManager::Instance().draw();

    // バックバッファをimgui用に準備
    m_dx12.prepareBackBufferForImGui();

    // imgui描画
    imguiRender();

    // 画面クリア後の後処理
    m_dx12.screenClearCleanup();

    // スクリーンショット実行（Scene 描画完了後、Present 後に実行）
    if (ScreenCapture::Instance().isCaptureRequested())
    {
        m_dx12.captureScreenshot();
    }

    // TimeManagerフレーム終了処理
    TimeManager::Instance().frameEnd(m_dx12.getGraphicsCommandList());
}

void Window::imguiRender()
{
    // imgui更新
    IMGUI_CTRL_UPDATE();

    // ログ描画
    Logger::Instance().renderLog();

    // TimeManagerのimgui描画
    TimeManager::Instance().imgui();

    // SceneManagerのimgui描画
    SceneManager::Instance().debugOption();

    // DX12のシーンimgui描画
    m_dx12.sceneImguiRender();

    // RenderManagerのimgui描画
    RenderManager::Instance().debugImgui();

    // deferred renderer のimgui描画
    GBufferRenderTargets::Instance().debugDrawImGui();

    // EditorManagerのimgui描画
    EditorManager::Instance().imgui();

    // imgui描画
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
            // 更新、描画
            update();
            render();
            updateTitleBar();
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT Window::processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // ImGui にメッセージを渡す
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