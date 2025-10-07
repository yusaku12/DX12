#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <memory>
#include "imgui_render.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

//@todo 一時的にimgui専用のアロケータを作っているが後々ちゃんとしたのを制作予定
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    ImVector<int>               FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        IM_ASSERT(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n - 1);
    }
    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        IM_ASSERT(FreeIndices.Size > 0);
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }
};

//=====================================================
// DX12 クラス
//=====================================================
class DX12
{
public:

    DX12(HWND hwnd);
    ~DX12() {};

    //! シングルトン取得
    static DX12& getInstance() { return *m_instance; };

    //! 画面をクリア
    void screenClear();

    //! 画面クリアの後処理
    void screenClearCleanup();

    //! 画面をリサイズ
    void screenResize(int width, int height);

    //! フェンスを待つ
    void safeGPUWait();

    //! スワップチェーン取得
    IDXGISwapChain4* getSwapChain() const { return m_dxgiSwapChain4.Get(); }

    //! デバイス取得
    ID3D12Device* getDevice() const { return m_device.Get(); }

    //! コマンドキュー取得
    ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue.Get(); }

    //! レンダーターゲットのディスクリプタヒープ
    ID3D12DescriptorHeap* getRTVDiscriptorHeap() const { return m_rtvHeaps.Get(); }

    //! シェーダーリソースのディスクリプタヒープ
    ID3D12DescriptorHeap* getSRVDiscriptorHeap() const { return m_srvHeaps.Get(); }

    //! コマンドリスト取得
    ID3D12GraphicsCommandList* getGraphicsCommandList() const { return m_graphicsCommandList.Get(); }

    //! imgui一時的なアロケータ
    ExampleDescriptorHeapAllocator getExampleDescriptorHeapAllocator() const { return m_exampleDescriptorHeapAllocator; }

    //! ハンドル取得
    HWND getHwnd() const { return m_hwnd; }

private:

    //! DX12で使用するデバッグ機能
    void enableDebugLayer();

    //! コマンドリセット
    void commandReset();

    static DX12* m_instance;
    const HWND m_hwnd;
    int m_width = 1280, m_height = 720;     //!< 画面の立幅、横幅
    static constexpr int BUFFER_COUNT = 3;  //!< バックバッファの数
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_dxgiSwapChain4;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeaps;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeaps;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D12CommandList> m_commandList;
    ExampleDescriptorHeapAllocator m_exampleDescriptorHeapAllocator;
    UINT64 m_fenceVall = 1;
};