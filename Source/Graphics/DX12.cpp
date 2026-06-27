#include "pch.h"
#include "Render/RenderPassContextFactory.h"
#include "GameObject\GameObject.h"
#include "Component\PostEffectComponent.h"
#include "Camera/CameraComponent.h"
#include "Editor/AssetDragDrop.h"

DX12* DX12::m_instance = nullptr;

DX12::DX12(HWND hwnd)
    : m_hwnd(hwnd)
{
    // インスタンス設定
    _ASSERT_EXPR(m_instance == nullptr, "already instantiated");
    m_instance = this;

    // 画面のサイズを取得する。
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT screenWidth = rc.right - rc.left;
    UINT screenHeight = rc.bottom - rc.top;

    this->m_width = screenWidth;
    this->m_height = screenHeight;

#ifdef _DEBUG
    // DX12で使用するデバッグ機能
    enableDebugLayer();
#endif

    // フィーチャレベル列挙
    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    // DXGI ファクトリ（IDXGIFactory） を作成
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_dxgiFactory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateDXGIFactory2");

    // NVIDIAのアダプタを探す（なければデフォルトアダプタ）
    Microsoft::WRL::ComPtr<IDXGIAdapter> selectedAdapter;
    for (UINT i = 0; ; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (m_dxgiFactory->EnumAdapters(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
            break;

        DXGI_ADAPTER_DESC desc = {};
        adapter->GetDesc(&desc);

        std::wstring wdesc = desc.Description;
        if (wdesc.find(L"NVIDIA") != std::wstring::npos)
        {
            selectedAdapter = adapter;
            break;
        }
    }

    // Direct3Dデバイスの初期化（順にトライ）
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    for (auto level : levels)
    {
        hr = D3D12CreateDevice(selectedAdapter.Get(), level, IID_PPV_ARGS(m_device.GetAddressOf()));
        if (SUCCEEDED(hr))
        {
            featureLevel = level;
            break;
        }
    }
    LOG_HR(hr, "Failed to create D3D12 device for any feature level");

#ifdef _DEBUG
    // Info Queue 設定（デバイス作成後）
    setupInfoQueue();
#endif

    // コマンドアロケーターを作成
    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandAllocator");

    // post-pass 用コマンドアロケーターを作成
    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_postCommandAllocator.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandAllocator (post)");

    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_postCommandAllocator2.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandAllocator (post2)");

    // コマンドリスト作成
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_graphicsCommandList.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandList");

    // コマンドキュー作成
    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cmdQueueDesc.NodeMask = 0;
    cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandQueue");

    // スワップチェイン作成
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = screenWidth;
    swapchainDesc.Height = screenHeight;
    swapchainDesc.Format = m_backBufferFormat;
    swapchainDesc.Stereo = false;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchainDesc.BufferCount = BUFFER_COUNT;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    hr = m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(),
        m_hwnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(m_dxgiSwapChain4.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateSwapChainForHwnd");
}

void DX12::initialize()
{
    if (!m_device) return;

    // RTVのディスクリプタヒープを作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NodeMask = 0;
    heapDesc.NumDescriptors = BUFFER_COUNT + 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeaps.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateDescriptorHeap(RTV)");

    // imgui用一時的なアロケータを作成
    {
        m_exampleDescriptorHeapAllocator.Create(m_device.Get(), DescriptorHeapManager::Instance().getHeap());
    }

    // スワップチェインに紐づけて RTV を作成
    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    hr = m_dxgiSwapChain4->GetDesc(&swcDesc);
    LOG_HR(hr, "GetDesc on swapchain failed");

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = swcDesc.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    for (UINT i = 0; i < swcDesc.BufferCount; ++i)
    {
        hr = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(m_backBuffers[i].GetAddressOf()));
        LOG_HR(hr, "GetBuffer failed for backbuffer");
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // シーン描画用 RenderTarget 作成
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = m_backBufferFormat;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = m_backBufferFormat;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.2f;
        clearValue.Color[2] = 0.4f;
        clearValue.Color[3] = 1.0f;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(m_sceneRenderTarget.GetAddressOf())
        );
        LOG_HR(hr, "Failed to create scene render target");
        m_sceneState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // RTVヒープの最後を Scene 用に使う
        m_sceneRTVHandle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        m_sceneRTVHandle.ptr += BUFFER_COUNT * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        m_device->CreateRenderTargetView(
            m_sceneRenderTarget.Get(),
            nullptr,
            m_sceneRTVHandle
        );

        // Scene を ImGui に渡すための SRV 作成
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = swcDesc.BufferDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        // SRVのインデックスを DescriptorHeapManager に割り当てて作成
        m_sceneSrvIndex = DescriptorHeapManager::Instance().allocateRange();
        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_sceneSrvIndex);
        m_device->CreateShaderResourceView(m_sceneRenderTarget.Get(), &srvDesc, cpuHandle);
        DescriptorHeapManager::Instance().syncToVisible(m_sceneSrvIndex);
    }

    // DSVヒープ作成
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = m_device->CreateDescriptorHeap(
            &dsvHeapDesc,
            IID_PPV_ARGS(m_dsvHeap.GetAddressOf())
        );
        LOG_HR(hr, "Failed to Create DSV Heap");

        m_dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    // 深度ステンシルバッファ作成
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(m_depthStencil.GetAddressOf())
        );
        LOG_HR(hr, "Failed to Create DepthStencil");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        //! DSV作成
        m_device->CreateDepthStencilView(
            m_depthStencil.Get(),
            &dsvDesc,
            m_dsvHandle
        );

        if (m_depthSrvIndex == UINT_MAX)
            m_depthSrvIndex = DescriptorHeapManager::Instance().allocateRange();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_depthSrvIndex);
        m_device->CreateShaderResourceView(m_depthStencil.Get(), &srvDesc, cpuHandle);
        DescriptorHeapManager::Instance().syncToVisible(m_depthSrvIndex);

        m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    // フェンスを作成(GPU側の処理が完了したか知るための仕組み)
    hr = m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateFence");
}

void DX12::screenClear(RenderPath renderPath)
{
    // DescriptorHeap
    DescriptorHeapManager::Instance().setDescriptorHeap();

    auto* cmd = m_graphicsCommandList.Get();

    transitionDepthToWrite();

    if (renderPath == RenderPath::Forward)
    {
        transitionSceneToRenderTarget();

        FLOAT clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
        cmd->ClearRenderTargetView(m_sceneRTVHandle, clearColor, 0, nullptr);

        cmd->ClearDepthStencilView(
            m_dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            1.0f,
            0,
            0,
            nullptr
        );

        cmd->OMSetRenderTargets(1, &m_sceneRTVHandle, FALSE, &m_dsvHandle);
        applyViewportAndScissor(cmd);
        return;
    }

    auto& gbuffer = GBufferRenderTargets::Instance();

    // GBuffer を RT に遷移 & クリア
    gbuffer.transitionToRenderTarget(cmd);
    gbuffer.clear(cmd);
    gbuffer.setRenderTargets(cmd, m_dsvHandle);

    // 深度ステンシルクリア
    m_graphicsCommandList->ClearDepthStencilView(
        m_dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f,
        0,
        0,
        nullptr
    );

    // ビューポートとシザー矩形をセット
    applyViewportAndScissor(m_graphicsCommandList.Get());
}

void DX12::transitionDepthToSRV()
{
    if (!m_depthStencil) return;
    if (m_depthState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthStencil.Get(),
        m_depthState,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    m_depthState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void DX12::transitionDepthToWrite()
{
    if (!m_depthStencil) return;
    if (m_depthState == D3D12_RESOURCE_STATE_DEPTH_WRITE) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthStencil.Get(),
        m_depthState,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void DX12::sceneImguiRender()
{
    ImGui::Begin("Scene");

    // シーンウィンドウがアクティブかどうか
    m_isSceneActive =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // 現在のウィンドウの DrawList を保存（ImGuizmo の入力判定に使う）
    m_sceneDrawList = ImGui::GetWindowDrawList();

    //! スクリーンショットボタン（Scene ウィンドウ上部に配置）
    if (ImGui::Button("Screenshot"))
    {
        ScreenCapture::Instance().requestCapture();
    }
    ImGui::SameLine();
    ImGui::Separator();

    ImTextureID texID = (ImTextureID)DescriptorHeapManager::Instance().getGPUHandle(m_finalPostEffectSrv).ptr;

    // ウィンドウサイズにフィットさせて表示
    ImGui::Image(texID, ImGui::GetContentRegionAvail());

    // Image のスクリーン座標を保存（ImGuizmo の SetRect に使う）
    ImVec2 itemMin = ImGui::GetItemRectMin();
    ImVec2 itemMax = ImGui::GetItemRectMax();
    m_sceneWindowPos = itemMin;
    m_sceneWindowSize = ImVec2(itemMax.x - itemMin.x, itemMax.y - itemMin.y);

    if (ImGui::BeginDragDropTarget())
    {
        EditorAssetDragDrop::acceptAssetDropInCurrentTarget(nullptr);
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

void DX12::transitionSceneToSRV()
{
    transitionSceneToSRV(m_graphicsCommandList.Get());
}

void DX12::transitionSceneToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (!m_sceneRenderTarget || !cmd) return;
    if (m_sceneState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sceneRenderTarget.Get(),
        m_sceneState,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmd->ResourceBarrier(1, &barrier);
    m_sceneState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void DX12::screenClearCleanup()
{
    // BackBuffer を PRESENT に戻す
    UINT bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();

    auto backBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_backBuffers[bbIdx].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    m_graphicsCommandList->ResourceBarrier(1, &backBarrier);

    // コマンドリストを閉じて実行
    executeCommandList();

    // フリップ処理
    m_dxgiSwapChain4->Present(1, 0);

    // GPU待機
    safeGPUWait();

#ifdef _DEBUG
    // DebugLayer の警告をフラッシュ
    flushDebugMessages();
#endif

    // コマンドリセット
    commandReset();
}

void DX12::screenResize(int width, int height)
{
    if (!m_device || !m_dxgiSwapChain4)
        return;

    if (width == 0 || height == 0)
        return;

    // GPU待機
    safeGPUWait();

    // コマンドリストを閉じる
    m_graphicsCommandList->Close();

    // 既存バックバッファ解放
    for (UINT i = 0; i < BUFFER_COUNT; ++i)
        m_backBuffers[i].Reset();

    // SceneRenderTarget 解放
    m_sceneRenderTarget.Reset();

    // SwapChain Resize
    DXGI_SWAP_CHAIN_DESC desc = {};
    m_dxgiSwapChain4->GetDesc(&desc);

    HRESULT hr = m_dxgiSwapChain4->ResizeBuffers(
        desc.BufferCount,
        width,
        height,
        desc.BufferDesc.Format,
        desc.Flags
    );
    LOG_HR(hr, "ResizeBuffers failed");

    // サイズ更新
    m_width = width;
    m_height = height;

    // BackBuffer 再取得 + RTV再生成
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();

    UINT rtvSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < BUFFER_COUNT; ++i)
    {
        hr = m_dxgiSwapChain4->GetBuffer(
            i,
            IID_PPV_ARGS(m_backBuffers[i].ReleaseAndGetAddressOf())
        );
        LOG_HR(hr, "GetBuffer failed after Resize");

        m_device->CreateRenderTargetView(
            m_backBuffers[i].Get(),
            nullptr,
            handle
        );

        handle.ptr += rtvSize;
    }

    // SceneRenderTarget 再生成
    {
        D3D12_RESOURCE_DESC rtDesc = {};
        rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rtDesc.Width = m_width;
        rtDesc.Height = m_height;
        rtDesc.DepthOrArraySize = 1;
        rtDesc.MipLevels = 1;
        rtDesc.Format = desc.BufferDesc.Format;
        rtDesc.SampleDesc.Count = 1;
        rtDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = desc.BufferDesc.Format;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.2f;
        clearValue.Color[2] = 0.4f;
        clearValue.Color[3] = 1.0f;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &rtDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(m_sceneRenderTarget.GetAddressOf())
        );
        LOG_HR(hr, "SceneRenderTarget recreate failed");
    }

    // 深度バッファ再生成
    {
        m_depthStencil.Reset();

        D3D12_RESOURCE_DESC Ddesc = {};
        Ddesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        Ddesc.Width = m_width;
        Ddesc.Height = m_height;
        Ddesc.DepthOrArraySize = 1;
        Ddesc.MipLevels = 1;
        Ddesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        Ddesc.SampleDesc.Count = 1;
        Ddesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        Ddesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &Ddesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(m_depthStencil.GetAddressOf())
        );
        LOG_HR(hr, "Depth recreate failed");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        m_device->CreateDepthStencilView(
            m_depthStencil.Get(),
            &dsvDesc,
            m_dsvHandle
        );

        if (m_depthSrvIndex == UINT_MAX)
            m_depthSrvIndex = DescriptorHeapManager::Instance().allocateRange();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_depthSrvIndex);
        m_device->CreateShaderResourceView(m_depthStencil.Get(), &srvDesc, cpuHandle);
        DescriptorHeapManager::Instance().syncToVisible(m_depthSrvIndex);

        m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    // Scene RTV 再作成
    m_sceneRTVHandle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    m_sceneRTVHandle.ptr += BUFFER_COUNT * rtvSize;

    m_device->CreateRenderTargetView(
        m_sceneRenderTarget.Get(),
        nullptr,
        m_sceneRTVHandle
    );

    // Scene SRV 再作成（リサイズ後に必須）
    {
        if (m_sceneSrvIndex == 0 || m_sceneSrvIndex == UINT_MAX)
            m_sceneSrvIndex = DescriptorHeapManager::Instance().allocateRange();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.BufferDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_sceneSrvIndex);
        m_device->CreateShaderResourceView(m_sceneRenderTarget.Get(), &srvDesc, cpuHandle);
        DescriptorHeapManager::Instance().syncToVisible(m_sceneSrvIndex);
    }

    // GBuffer / PostEffect リサイズ
    DeferredRenderer::Instance().resize(m_width, m_height);
    PostEffectRenderTargets::Instance().resize(m_width, m_height);

    // コマンドリストを再オープン
    commandReset();
}

void DX12::enableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> debugLayer;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf()))))
    {
        debugLayer->EnableDebugLayer();

        // GPU-Based Validation を有効化（重いがバグ発見に非常に有効）
        Microsoft::WRL::ComPtr<ID3D12Debug3> debug3;
        if (SUCCEEDED(debugLayer.As(&debug3)))
        {
            debug3->SetEnableGPUBasedValidation(TRUE);
        }
    }
}

void DX12::setupInfoQueue()
{
#ifdef _DEBUG
    if (SUCCEEDED(m_device.As(&m_infoQueue)))
    {
        // ERROR と CORRUPTION は例外で停止
        m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

        // 不要な警告を抑制するフィルタ
        D3D12_MESSAGE_ID denyIds[] =
        {
            // リソースバリアの冗長な遷移警告
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        };

        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        m_infoQueue->AddStorageFilterEntries(&filter);
    }
#endif
}

void DX12::flushDebugMessages()
{
#ifdef _DEBUG
    if (!m_infoQueue) return;

    UINT64 messageCount = m_infoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < messageCount; ++i)
    {
        SIZE_T messageLength = 0;
        m_infoQueue->GetMessage(i, nullptr, &messageLength);

        std::vector<char> buffer(messageLength);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
        m_infoQueue->GetMessage(i, message, &messageLength);

        // 重大度に応じて出力
        switch (message->Severity)
        {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        case D3D12_MESSAGE_SEVERITY_ERROR:
            OutputDebugStringA("[DX12 ERROR] ");
            break;
        case D3D12_MESSAGE_SEVERITY_WARNING:
            OutputDebugStringA("[DX12 WARNING] ");
            break;
        case D3D12_MESSAGE_SEVERITY_INFO:
            OutputDebugStringA("[DX12 INFO] ");
            break;
        default:
            OutputDebugStringA("[DX12] ");
            break;
        }
        OutputDebugStringA(message->pDescription);
        OutputDebugStringA("\n");
    }

    m_infoQueue->ClearStoredMessages();
#endif
}

void DX12::safeGPUWait()
{
    // Signal により fence に値を設定
    const UINT64 fenceValueToWait = ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), fenceValueToWait);

    // 完了値が待ち値より小さい場合のみ待つ
    if (m_fence->GetCompletedValue() < fenceValueToWait)
    {
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (event == nullptr) LOG_ASSERT_NO_JUDGE("CreateEvent failed in safeGPUWait");

        m_fence->SetEventOnCompletion(fenceValueToWait, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
}

void DX12::prepareBackBufferForImGui()
{
    // コマンドリストを閉じて GPU に送る（Scene 描画が完了している状態にする）
    executeCommandList();

    // ワーカーコマンドリストをメインの前に実行
    auto workerLists = CommandListPool::Instance().getClosedCommandLists();
    if (!workerLists.empty())
    {
        m_commandQueue->ExecuteCommandLists(
            static_cast<UINT>(workerLists.size()),
            workerLists.data());

        const UINT64 fenceValue = ++m_fenceValue;
        m_commandQueue->Signal(m_fence.Get(), fenceValue);
        CommandListPool::Instance().notifySubmitted(fenceValue);
    }
    CommandListPool::Instance().resetCompleted();

    // post-pass 用のアロケータでリセット（pre-pass アロケータは GPU 使用中のため触らない）
    m_postCommandAllocator->Reset();
    m_graphicsCommandList->Reset(m_postCommandAllocator.Get(), nullptr);

    RenderPassContext context = BuildRenderPassContext(
        SceneManager::Instance().isCurrentSceneMultiThreadedRenderingEnabled(),
        m_sceneSrvIndex);

    RenderPipeline::Instance().execute(context, RenderPassStage::BeforePostEffect);

    if (context.requestWorkerFlush)
    {
        executeCommandList();

        auto forwardLists = CommandListPool::Instance().getClosedCommandLists();
        if (!forwardLists.empty())
        {
            m_commandQueue->ExecuteCommandLists(
                static_cast<UINT>(forwardLists.size()),
                forwardLists.data());

            const UINT64 fenceValue = ++m_fenceValue;
            m_commandQueue->Signal(m_fence.Get(), fenceValue);
            CommandListPool::Instance().notifySubmitted(fenceValue);
        }
        CommandListPool::Instance().resetCompleted();

        m_postCommandAllocator2->Reset();
        m_graphicsCommandList->Reset(m_postCommandAllocator2.Get(), nullptr);
    }

    // Scene RT を SRV に遷移（ImGui 描画前に呼ぶ）
    transitionSceneToSRV();

    RenderPipeline::Instance().execute(context, RenderPassStage::PostEffect);

    m_finalPostEffectSrv = context.finalSrvIndex;

    UINT bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();

    // PRESENT → RENDER_TARGET
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_backBuffers[bbIdx].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    m_graphicsCommandList->ResourceBarrier(1, &barrier);

    // RTV取得
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    UINT rtvSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rtvHandle.ptr += bbIdx * rtvSize;

    FLOAT clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    m_graphicsCommandList->ClearRenderTargetView(
        rtvHandle,
        clearColor,
        0,
        nullptr
    );

    m_graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHandle);
}

void DX12::executeCommandList()
{
    // コマンドリストを閉じて実行
    m_graphicsCommandList->Close();
    ID3D12CommandList* lists[] = { m_graphicsCommandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);
}

void DX12::applyViewportAndScissor(ID3D12GraphicsCommandList* cmd) const
{
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<FLOAT>(m_width);
    viewport.Height = static_cast<FLOAT>(m_height);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MaxDepth = 1.0f;
    viewport.MinDepth = 0.0f;
    cmd->RSSetViewports(1, &viewport);

    D3D12_RECT scissorrect = {};
    scissorrect.top = 0;
    scissorrect.left = 0;
    scissorrect.right = scissorrect.left + m_width;
    scissorrect.bottom = scissorrect.top + m_height;
    cmd->RSSetScissorRects(1, &scissorrect);
}

void DX12::applySceneRenderTargets(ID3D12GraphicsCommandList* cmd) const
{
    cmd->OMSetRenderTargets(1, &m_sceneRTVHandle, FALSE, &m_dsvHandle);
}

void DX12::commandReset()
{
    // 領域をクリア、次フレーム用に命令を積める状態にする
    m_commandAllocator->Reset();
    m_postCommandAllocator->Reset();
    m_postCommandAllocator2->Reset();
    m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr);
}

void DX12::captureScreenshot()
{
    //! スクリーンショット用に専用コマンドアロケータ＆コマンドリストを作成
    //! メインのコマンドリストを汚さないようにする
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> captureAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> captureCmd;

    HRESULT hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(captureAllocator.GetAddressOf()));
    LOG_HR(hr, "ScreenCapture: コマンドアロケータ作成失敗");
    if (FAILED(hr)) return;

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        captureAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(captureCmd.GetAddressOf()));
    LOG_HR(hr, "ScreenCapture: コマンドリスト作成失敗");
    if (FAILED(hr)) return;

    //! Scene RT は SRV 状態（PIXEL_SHADER_RESOURCE）にあるのでそれを渡す
    ScreenCapture::Instance().capture(
        m_sceneRenderTarget.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        captureCmd.Get(),
        m_commandQueue.Get(),
        m_device.Get());

    //! GPU待機して完全にコピーが完了するのを待つ
    safeGPUWait();
}

void DX12::transitionSceneToRenderTarget()
{
    transitionSceneToRenderTarget(m_graphicsCommandList.Get());
}

void DX12::transitionSceneToRenderTarget(ID3D12GraphicsCommandList* cmd)
{
    if (!m_sceneRenderTarget || !cmd) return;
    if (m_sceneState == D3D12_RESOURCE_STATE_RENDER_TARGET) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sceneRenderTarget.Get(),
        m_sceneState,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmd->ResourceBarrier(1, &barrier);
    m_sceneState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}