#pragma once

//========================================================
// SRV / CBV / UAV を一括管理
// シェーダー可視 DescriptorHeap を内部で保持
// インデックス割り当てもここで行う
//========================================================
class DescriptorHeapManager
{
public:

    //! シングルトン
    static DescriptorHeapManager& Instance()
    {
        static DescriptorHeapManager instance;
        return instance;
    }

    //! 割り当て禁止（シングルトン）
    DescriptorHeapManager(const DescriptorHeapManager&) = delete;
    void operator=(const DescriptorHeapManager&) = delete;

    //! 初期化（アプリ開始時に1回呼ぶ）
    void initialize(UINT maxCount = 1024);

    //! SRV作成
    UINT createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);

    //! CBV作成
    UINT createCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc);

    //! UAV作成
    UINT createUAV(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc);

    //! DescriptorHeapとGPUハンドル取得
    ID3D12DescriptorHeap* getHeap() { return m_heap.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT index);

    //! 解放（使わなくなった Descriptor を返却）
    void free(UINT index);

private:

    DescriptorHeapManager() {}

    // ! インデックス割り当て
    UINT allocate();

    // ! CPU ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT index);

private:

    std::vector<bool> m_used;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    UINT m_maxCount = 0;
    UINT m_incrementSize = 0;
};