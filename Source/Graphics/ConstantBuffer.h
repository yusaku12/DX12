#pragma once

//=====================================================
// 定数バッファ管理クラス（テンプレート版）
//=====================================================
template<typename T>
class ConstantBuffer
{
public:

    //! コンストラクタでCB生成
    ConstantBuffer()
    {
        auto device = DX12::Instance().getDevice();

        //! 256バイトアライン
        m_cbSize = (sizeof(T) + 255) & ~255;

        HRESULT hr = device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(m_cbSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_constantBuffer.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) return;

        //! Map（永続Map）
        hr = m_constantBuffer->Map(0, nullptr, &m_mappedCB);
        if (FAILED(hr))
        {
            m_mappedCB = nullptr;
            return;
        }

        //! CBV 作成
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = m_cbSize;

        //! DescriptorHeap に登録（index は Manager 側で管理）
        m_cbvIndex = DescriptorHeapManager::Instance().createCBV(cbvDesc);
    }

    //! デストラクタ
    ~ConstantBuffer()
    {
        if (m_constantBuffer && m_mappedCB)
        {
            m_constantBuffer->Unmap(0, nullptr);
            m_mappedCB = nullptr;
        }
    }

    //! コンスタントバッファ更新
    void update(const T& data)
    {
        memcpy(m_mappedCB, &data, sizeof(T));
    }

    //! GPUハンドル取得（描画時用）
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const
    {
        return DescriptorHeapManager::Instance().getGPUHandle(m_cbvIndex);
    }

private:

    UINT m_cbSize = 0;
    void* m_mappedCB = nullptr;

    UINT m_cbvIndex = 0;  //!< DescriptorHeap上のCBVインデックス
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
};