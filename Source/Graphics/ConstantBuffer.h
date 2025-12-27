#pragma once

//=====================================================
// 定数バッファ管理クラス（テンプレート版）
//=====================================================
template<typename T>
class ConstantBuffer
{
public:

    //! コンストラクタでCB生成
    ConstantBuffer(UINT elementCount = 1)
        : m_elementCount(elementCount)
    {
        auto device = DX12::Instance().getDevice();

        m_elementSize = (sizeof(T) + 255) & ~255;
        m_bufferSize = m_elementSize * m_elementCount;

        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_bufferSize);
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_buffer.ReleaseAndGetAddressOf())
        );
        LOG_HR(hr, "[ConstantBuffer] Failed to create constant buffer resource.");

        hr = m_buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mapped));
        if (FAILED(hr))
        {
            m_mapped = nullptr;
            return;
        }

        //! CBV 作成
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = m_buffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = m_bufferSize;

        m_cbvIndex = DescriptorHeapManager::Instance().createCBV(cbvDesc);
    }

    //! デストラクタ
    ~ConstantBuffer()
    {
        //! 何故かデストラクタでUnmapすると落ちるのでコメントアウト
        //if (m_buffer && m_mapped)
        //{
        //    m_buffer->Unmap(0, nullptr);
        //    m_mapped = nullptr;
        //}
    }

    //! 単体用（index = 0）
    void update(const T& data)
    {
        memcpy(m_mapped, &data, sizeof(T));
    }

    //! 配列用
    void update(UINT index, const T& data)
    {
        memcpy(m_mapped + index * m_elementSize, &data, sizeof(T));
    }

    //! GPUハンドル取得（描画時用）
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const
    {
        return DescriptorHeapManager::Instance().getGPUHandle(m_cbvIndex);
    }

private:

    UINT m_elementCount = 0;
    UINT m_elementSize = 0;
    UINT m_bufferSize = 0;

    UINT m_cbvIndex = 0;

    uint8_t* m_mapped = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;
};