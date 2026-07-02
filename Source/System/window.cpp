#include "pch.h"
#include "UI/UIRenderer.h"
#include "UI/UIFontManager.h"
#include "System/TimeManager.h"
#include "Scene/SceneManager.h"
#include "Render/RenderManager.h"
#include "Render/RenderPipeline.h"
#include "Render/DeferredRenderer.h"
#include "Render/GBufferRenderTargets.h"
#include "Render/ShadowMapRenderer.h"
#include "Input/InputManager.h"
#include "GameObject/GameObjectRegistry.h"
#include "Camera/CameraManager.h"
#include "Audio/AudioManager.h"
#include "Editor/EditorManager.h"
#include "Graphics/IBLManager.h"
#include "PostEffect/PostEffectRenderTargets.h"
#include "Window.h"
#include "System\EventBus.h"
#include "System\RuntimeUIManager.h"

Window::Window(HWND hwnd)
    : m_hwnd(hwnd)
    , m_dx12(hwnd)
{
    initializeEngineSubsystems();
}

Window::~Window()
{
    // GPU 完了待ち
    m_dx12.safeGPUWait();

    shutdownEngineSubsystems();
}

void Window::initializeEngineSubsystems()
{
    if (m_engineSubsystemsInitialized)
    {
        return;
    }

    // GPU/Descriptor 系の基盤を先に構築
    DescriptorHeapManager::Instance().initialize();
    m_dx12.initialize();
    MemorySystem::Instance().bindDevice(m_dx12.getDevice());
    GBufferRenderTargets::Instance().initialize();
    CommandListPool::Instance().initialize(m_dx12.getDevice(), m_dx12.getFence(), 4);

    // デバッグ/UI 系
    IMGUI_CTRL_INITIALIZE();

    // グラフィックス依存マネージャー群
    ShaderManager::Instance().initialize();
    PiplineState::Instance().initialize();
    RootSignatureManager::Instance().initialize();
    IBLManager::Instance().initialize();
    PostEffectRenderTargets::Instance().initialize();
    DeferredRenderer::Instance().initialize();
    ShadowMapRenderer::Instance().initialize();
    RenderPipeline::Instance().initialize();

    // コアランタイム系
    TimeManager::Instance().initialize();
    CameraManager::Instance().initialize();
    AudioManager::Instance().initialize();
    DebugPrimitive::Instance().initialize();
    PhysicsWorld::Instance().initialize();
    SceneManager::Instance().initialize();
    RuntimeUIManager::Instance().initialize();

    // ネイティブ UI サブシステム（シェーダー・フォントアトラスを準備）
    UIRenderer::Instance().initialize();
    UIFontManager::Instance().initialize();

    m_engineSubsystemsInitialized = true;
}

void Window::shutdownEngineSubsystems()
{
    if (!m_engineSubsystemsInitialized)
    {
        return;
    }

    // 初期化の逆順で終了処理を行う（shutdown API を持つもののみ）

    // 1) シーン/オブジェクト
    SceneManager::Instance().shutdown();
    GameObjectRegistry::Instance().shutdown();
    RuntimeUIManager::Instance().shutdown();

    // ネイティブ UI サブシステム
    UIRenderer::Instance().shutdown();
    UIFontManager::Instance().shutdown();

    // 2) ランタイム系（Scene に依存しうるものから順に停止）
    PhysicsWorld::Instance().shutdown();
    DebugPrimitive::Instance().shutdown();
    AudioManager::Instance().shutdown();
    CameraManager::Instance().shutdown();

    // 3) 描画コンポーネント登録情報の解放
    RenderManager::Instance().shutdown();

    // ImGui がある場合は先に終了処理
    IMGUI_CTRL_FINALIZE();

    m_engineSubsystemsInitialized = false;
}

void Window::update()
{
    // TimeManager更新
    TimeManager::Instance().update();

    // shaderManager更新
    ShaderManager::Instance().update();

    // テクスチャストリーミング更新
    TextureManager::Instance().updateStreaming();

    // InputManager更新
    InputManager::Instance().update();

    // デバックプリミティブ フレーム開始（前フレームの描画リクエストをクリア）
    DebugPrimitive::Instance().beginFrame();

    // SceneManager更新
    SceneManager::Instance().update();

    // ゲームオブジェクト更新（FreeCameraComponent など、カメラ位置を動かすコンポーネントを先に更新）
    GameObjectRegistry::Instance().update();

    // ランタイムUI更新（ObjectPicker より先にクリックを判定する）
    RuntimeUIManager::Instance().update();
    EventBus::Instance().dispatchQueued();

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

    // ネイティブ UI をシーン RT に描画（PostEffect 後、ImGui 前）
    RuntimeUIManager::Instance().renderNative();

    // バックバッファをimgui用に準備
    m_dx12.prepareBackBufferForImGui();

    // imgui描画
    imguiRender();

    // TimeManagerフレーム終了処理（コマンド実行前に同一コマンドリストへ記録）
    TimeManager::Instance().frameEnd(m_dx12.getGraphicsCommandList());

    // 画面クリア後の後処理
    m_dx12.screenClearCleanup();

    // スクリーンショット実行（Scene 描画完了後、Present 後に実行）
    if (ScreenCapture::Instance().isCaptureRequested())
    {
        m_dx12.captureScreenshot();
    }
}

void Window::imguiRender()
{
    // imgui更新
    IMGUI_CTRL_UPDATE();

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