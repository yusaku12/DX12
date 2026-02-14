#include "pch.h"

DX12* DX12::m_instance = nullptr;

DX12::DX12(HWND hwnd)
    :m_hwnd(hwnd)
{
    //! インスタンス設定
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
    //! DX12で使用するデバッグ機能
    enableDebugLayer();
#endif

    //! フィーチャレベル列挙
    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    //! DXGI ファクトリ（IDXGIFactory） を作成
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_dxgiFactory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateDXGIFactory2");

    //! NVIDIAのアダプタを探す
    Microsoft::WRL::ComPtr<IDXGIAdapter>selectedAdapter;
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

    //! Direct3Dデバイスの初期化
    D3D_FEATURE_LEVEL featureLevel;
    for (auto level : levels)
    {
        hr = D3D12CreateDevice(selectedAdapter.Get(), level, IID_PPV_ARGS(m_device.GetAddressOf()));
        if (SUCCEEDED(hr))
        {
            featureLevel = level;
            break;
        }
    }

    //! コマンドアロケーターを作成
    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandAllocator");

    //! コマンドリスト作成
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_graphicsCommandList.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandList");

    //! コマンドキュー作成
    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;         //!< タイムアウトなし
    cmdQueueDesc.NodeMask = 0;                                  //!< アダプターを1つしか使わないときは0でいい
    cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//!< プライオリティ特に指定なし
    cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;         //!< ここはコマンドリストと合わせる
    hr = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateCommandQueue");

    //! スワップチェイン作成
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = screenWidth;
    swapchainDesc.Height = screenHeight;
    swapchainDesc.Format = m_backBufferFormat;  //!< HDR の場合は DXGI_FORMAT_R16G16B16A16_FLOAT
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
    //! RTVのディスクリプタヒープを作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;  //! レンダーターゲットビューなのでRTV
    heapDesc.NodeMask = 0;
    heapDesc.NumDescriptors = BUFFER_COUNT + 1;      //! BufferCountの数に合わせる
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//! シェーダーからデータを読み取るわけでは無いのでNONE
    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeaps.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateDescriptorHeap");

    //! imgui用一時的なアロケータを作成
    m_exampleDescriptorHeapAllocator.Create(m_device.Get(), DescriptorHeapManager::Instance().getHeap());  // @todo imgui用一時的なアロケータ

    //! スワップチェインに紐づけて RTV を作成
    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    hr = m_dxgiSwapChain4->GetDesc(&swcDesc);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = swcDesc.BufferDesc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    for (UINT i = 0; i < swcDesc.BufferCount; ++i)
    {
        hr = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(m_backBuffers[i].GetAddressOf()));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! シーン描画用 RenderTarget 作成
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

        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(m_sceneRenderTarget.GetAddressOf())
        );

        //! RTVヒープの最後を Scene 用に使う
        m_sceneRTVHandle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        m_sceneRTVHandle.ptr += BUFFER_COUNT * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        m_device->CreateRenderTargetView(
            m_sceneRenderTarget.Get(),
            nullptr,
            m_sceneRTVHandle
        );

        //! Scene を ImGui に渡すための SRV 作成
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = swcDesc.BufferDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        //! SRVのインデックスを DescriptorHeapManager に割り当てて作成
        m_sceneSrvIndex = DescriptorHeapManager::Instance().allocate();
        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_sceneSrvIndex);
        m_device->CreateShaderResourceView(m_sceneRenderTarget.Get(), &srvDesc, cpuHandle);
    }

    //! フェンスを作成(GPU側の処理が完了したか知るための仕組み)
    hr = m_device->CreateFence(m_fenceVall, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
    LOG_HR(hr, "Failed to CreateFence");
}

void DX12::screenClear()
{
    //! Scene用バリア
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sceneRenderTarget.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    m_graphicsCommandList->ResourceBarrier(1, &barrier);

    //! DescriptorHeap
    DescriptorHeapManager::Instance().setDiscriptorHeap();

    //! SceneRTVをセット
    m_graphicsCommandList->OMSetRenderTargets(1, &m_sceneRTVHandle, FALSE, nullptr);

    //! Sceneクリア
    FLOAT clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_graphicsCommandList->ClearRenderTargetView(
        m_sceneRTVHandle,
        clearColor,
        0,
        nullptr
    );

    //! ビューポート設定
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<FLOAT>(m_width);
    viewport.Height = static_cast<FLOAT>(m_height);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MaxDepth = 1.0f;
    viewport.MinDepth = 0.0f;
    m_graphicsCommandList->RSSetViewports(1, &viewport);

    //! シーザーラクト設定
    D3D12_RECT scissorrect = {};
    scissorrect.top = 0;
    scissorrect.left = 0;
    scissorrect.right = scissorrect.left + m_width;
    scissorrect.bottom = scissorrect.top + m_height;
    m_graphicsCommandList->RSSetScissorRects(1, &scissorrect);
}

void DX12::sceneImguiRender()
{
    ImGui::Begin("Scene");

    //! シーンウィンドウがアクティブかどうか
    m_isSceneActive =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    ImTextureID texID = (ImTextureID)DescriptorHeapManager::Instance().getGPUHandle(m_sceneSrvIndex).ptr;

    //! ウィンドウサイズにフィットさせて表示
    ImGui::Image(texID, ImGui::GetContentRegionAvail());

    ImGui::End();
}

void DX12::renderTargetUndo()
{
    //! Scene を SRV に戻す
    auto sceneBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sceneRenderTarget.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    m_graphicsCommandList->ResourceBarrier(1, &sceneBarrier);

    //! BackBuffer を PRESENT に戻す
    UINT bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();

    auto backBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_backBuffers[bbIdx].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    m_graphicsCommandList->ResourceBarrier(1, &backBarrier);
}

void DX12::screenClearCleanup()
{
    //! コマンドリストを閉じて実行
    executeCommandList();

    //! フリップ処理
    m_dxgiSwapChain4->Present(1, 0);

    //! GPU待機
    safeGPUWait();

    //! コマンドリセット
    commandReset();
}

void DX12::screenResize(int width, int height)
{
    if (!m_device || !m_dxgiSwapChain4)
        return;

    if (width == 0 || height == 0)
        return;

    //! GPU待機
    safeGPUWait();

    //! コマンドリストを閉じる
    m_graphicsCommandList->Close();

    //! 既存バックバッファ解放
    for (UINT i = 0; i < BUFFER_COUNT; ++i)
        m_backBuffers[i].Reset();

    //! SceneRenderTarget 解放
    m_sceneRenderTarget.Reset();

    //! SwapChain Resize
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

    //! サイズ更新
    m_width = width;
    m_height = height;

    //! BackBuffer 再取得 + RTV再生成
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

    //! SceneRenderTarget 再生成
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

    //! Scene RTV 再作成
    m_sceneRTVHandle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    m_sceneRTVHandle.ptr += BUFFER_COUNT * rtvSize;

    m_device->CreateRenderTargetView(
        m_sceneRenderTarget.Get(),
        nullptr,
        m_sceneRTVHandle
    );

    //! Scene SRV 再作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.BufferDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    //! DescriptorHeapManager を使って SRV を作成し、インデックスを取得
    auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_sceneSrvIndex);
    m_device->CreateShaderResourceView(m_sceneRenderTarget.Get(), &srvDesc, cpuHandle);

    //! コマンドリセット
    commandReset();
}

void DX12::enableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> debugLayer;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf()))))
    {
        debugLayer->EnableDebugLayer();
    }
}

void DX12::safeGPUWait()
{
    //! Signal により fence に値を設定
    const UINT64 fenceValueToWait = ++m_fenceVall;
    m_commandQueue->Signal(m_fence.Get(), fenceValueToWait);

    //! 完了値が待ち値より小さい場合のみ待つ
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
    UINT bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();

    //! PRESENT → RENDER_TARGET
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_backBuffers[bbIdx].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    m_graphicsCommandList->ResourceBarrier(1, &barrier);

    //! RTV取得
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

    m_graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
}

void DX12::executeCommandList()
{
    //! コマンドリストを閉じて実行
    m_graphicsCommandList->Close();
    ID3D12CommandList* lists[] = { m_graphicsCommandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(lists), lists);
}

void DX12::commandReset()
{
    //! 領域をクリア、次フレーム用に命令を積める状態にする
    m_commandAllocator->Reset();
    m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr);
}