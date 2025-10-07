#include "dx12.h"
#include <stdexcept>
#include <string>
#include "System\logger.h"

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
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    //! DXGI ファクトリ（IDXGIFactory） を作成
    HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_dxgiFactory));

    //! NVIDIAのアダプタを探す
    Microsoft::WRL::ComPtr<IDXGIAdapter>selectedAdapter;
    for (UINT i = 0; ; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (m_dxgiFactory->EnumAdapters(i, &adapter) == DXGI_ERROR_NOT_FOUND)
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
        result = D3D12CreateDevice(selectedAdapter.Get(), level, IID_PPV_ARGS(&m_device));
        if (SUCCEEDED(result))
        {
            featureLevel = level;
            break;
        }
    }

    //! コマンドリストとコマンドアロケーターを作成
    result = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    result = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_graphicsCommandList));

    //! コマンドキュー作成
    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;         //!< タイムアウトなし
    cmdQueueDesc.NodeMask = 0;                                  //!< アダプターを1つしか使わないときは0でいい
    cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//!< プライオリティ特に指定なし
    cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;         //!< ここはコマンドリストと合わせる
    result = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));

    //! スワップチェイン作成
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = screenWidth;
    swapchainDesc.Height = screenHeight;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  //!< HDR の場合は DXGI_FORMAT_R16G16B16A16_FLOAT
    swapchainDesc.Stereo = false;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapchainDesc.BufferCount = BUFFER_COUNT;
    swapchainDesc.Scaling = DXGI_SCALING_NONE;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    result = m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(),
        m_hwnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(m_dxgiSwapChain4.GetAddressOf()));

    //! RTVのディスクリプタヒープを作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;  //! レンダーターゲットビューなのでRTV
    heapDesc.NodeMask = 0;
    heapDesc.NumDescriptors = BUFFER_COUNT;          //! BufferCountの数に合わせる
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//! シェーダーからデータを読み取るわけでは無いのでNONE
    result = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeaps));

    //! SRVのディスクリプタヒープを作成
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.NodeMask = 0;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeaps));
    m_exampleDescriptorHeapAllocator.Create(m_device.Get(), m_srvHeaps.Get());  // @todo imgui用一時的なアロケータ

    //! スワップチェーンと紐づけを行う
    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    result = m_dxgiSwapChain4->GetDesc(&swcDesc);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < swcDesc.BufferCount; ++i)
    {
        result = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! フェンスを作成(GPU側の処理が完了したか知るための仕組み)
    result = m_device->CreateFence(m_fenceVall, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
}

void DX12::screenClear()
{
    //! バックバッファのインデックスを取得
    //! リソースバリアを作成ー＞バックバッファの状態を遷移
    auto bbIdx = m_dxgiSwapChain4->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER BarrierDesc = {};
    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    BarrierDesc.Transition.pResource = m_backBuffers[bbIdx].Get();
    BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_graphicsCommandList->ResourceBarrier(1, &BarrierDesc);

    auto rtvH = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

    //画面クリア
    float clearColor[] = { 0.4f,0.4f,0.7f,1.0f };
    m_graphicsCommandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

    //! レンダーターゲットを指定
    m_graphicsCommandList->OMSetRenderTargets(1, &rtvH, false, nullptr);
    m_graphicsCommandList->SetDescriptorHeaps(1, m_srvHeaps.GetAddressOf());

    //! imguiの描画情報を設定
    IMGUI_CTRL_RENDER_INFO();

    //! 画面を遷移した後に切り替える元に戻すが正しい(めんどくさい)
    BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_graphicsCommandList->ResourceBarrier(1, &BarrierDesc);

    //! 命令の記録は終わり
    m_graphicsCommandList->Close();

    //! コマンドリストの実行
    m_commandList = m_graphicsCommandList;
    m_commandQueue->ExecuteCommandLists(1, m_commandList.GetAddressOf());
}

void DX12::screenClearCleanup()
{
    //! GPU待機
    safeGPUWait();

    //! コマンドリセット
    commandReset();

    //! フリップ処理
    m_dxgiSwapChain4->Present(1, 0);
}

void DX12::screenResize(int width, int height)
{
    if (!m_device || !m_dxgiSwapChain4) return;

    //! コマンドリストを Close（記録中なら必ず閉じる）
    m_graphicsCommandList->Close();

    //! GPU待機
    safeGPUWait();

    //! バックバッファを解放
    for (UINT i = 0; i < BUFFER_COUNT; ++i)
    {
        m_backBuffers[i].Reset();
    }

    //! バッファをリサイズ
    DXGI_SWAP_CHAIN_DESC desc = {};
    m_dxgiSwapChain4->GetDesc(&desc);
    HRESULT hr = m_dxgiSwapChain4->ResizeBuffers(
        desc.BufferCount,
        width,
        height,
        desc.BufferDesc.Format,
        desc.Flags
    );
    if (FAILED(hr)) throw std::runtime_error("ResizeBuffers failed");

    //! RTV再生成
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < desc.BufferCount; ++i)
    {
        hr = m_dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        if (FAILED(hr)) throw std::runtime_error("GetBuffer failed");

        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
        handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    //! コマンドリセット
    commandReset();
}

void DX12::enableDebugLayer()
{
    ID3D12Debug* debugLayer = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer))))
    {
        debugLayer->EnableDebugLayer();
        debugLayer->Release();
    }
}

void DX12::safeGPUWait()
{
    UINT64 fenceValueToWait = ++m_fenceVall;

    m_commandQueue->Signal(m_fence.Get(), fenceValueToWait);

    UINT64 completed = m_fence->GetCompletedValue();
    if (completed >= fenceValueToWait) return; // GPU 完了済みなら待たない

    HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle) throw std::runtime_error("Failed to create event");

    HRESULT hr = m_fence->SetEventOnCompletion(fenceValueToWait, eventHandle);
    if (FAILED(hr))
    {
        CloseHandle(eventHandle);
        throw std::runtime_error("Failed to set fence event");
    }

    WaitForSingleObject(eventHandle, INFINITE);
    CloseHandle(eventHandle);
}

void DX12::commandReset()
{
    //! 領域をクリア、次フレーム用に命令を積める状態にする
    m_commandAllocator->Reset();
    m_graphicsCommandList->Reset(m_commandAllocator.Get(), nullptr);
}