#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <memory>
#include "imgui_render.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

//@todo �ꎞ�I��imgui��p�̃A���P�[�^������Ă��邪��X�����Ƃ����̂𐧍�\��
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
// DX12 �N���X
//=====================================================
class DX12
{
public:

    DX12(HWND hwnd);
    ~DX12() {};

    //! �V���O���g���擾
    static DX12& getInstance() { return *m_instance; };

    //! ��ʂ��N���A
    void screenClear();

    //! ��ʃN���A�̌㏈��
    void screenClearCleanup();

    //! ��ʂ����T�C�Y
    void screenResize(int width, int height);

    //! �t�F���X��҂�
    void safeGPUWait();

    //! �X���b�v�`�F�[���擾
    IDXGISwapChain4* getSwapChain() const { return m_dxgiSwapChain4.Get(); }

    //! �f�o�C�X�擾
    ID3D12Device* getDevice() const { return m_device.Get(); }

    //! �R�}���h�L���[�擾
    ID3D12CommandQueue* getCommandQueue() const { return m_commandQueue.Get(); }

    //! �����_�[�^�[�Q�b�g�̃f�B�X�N���v�^�q�[�v
    ID3D12DescriptorHeap* getRTVDiscriptorHeap() const { return m_rtvHeaps.Get(); }

    //! �V�F�[�_�[���\�[�X�̃f�B�X�N���v�^�q�[�v
    ID3D12DescriptorHeap* getSRVDiscriptorHeap() const { return m_srvHeaps.Get(); }

    //! �R�}���h���X�g�擾
    ID3D12GraphicsCommandList* getGraphicsCommandList() const { return m_graphicsCommandList.Get(); }

    //! imgui�ꎞ�I�ȃA���P�[�^
    ExampleDescriptorHeapAllocator getExampleDescriptorHeapAllocator() const { return m_exampleDescriptorHeapAllocator; }

    //! �n���h���擾
    HWND getHwnd() const { return m_hwnd; }

private:

    //! DX12�Ŏg�p����f�o�b�O�@�\
    void enableDebugLayer();

    //! �R�}���h���Z�b�g
    void commandReset();

    static DX12* m_instance;
    const HWND m_hwnd;
    int m_width = 1280, m_height = 720;     //!< ��ʂ̗����A����
    static constexpr int BUFFER_COUNT = 3;  //!< �o�b�N�o�b�t�@�̐�
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