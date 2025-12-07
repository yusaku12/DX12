#pragma once

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// LoadTexture クラス
//=====================================================
class LoadTexture
{
public:

    explicit LoadTexture(const wchar_t* filename);
    ~LoadTexture() {}

    //! SRV を格納したディスクリプタヒープを返す
    ID3D12DescriptorHeap* GetHeap() const { return m_srvHeap.Get(); }

    //! GPU ハンドルを返す（root parameter へ設定するときに使用）
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_srvHeap->GetGPUDescriptorHandleForHeapStart(); }

private:

    void loadTexture(const wchar_t* filename);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
};