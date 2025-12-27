#pragma once

//=====================================================
// インデックスデータ作成クラス（テンプレート版）
//=====================================================
template<typename T>
class IndexBuffer
{
public:

    //! コンストラクタでインデックスバッファ生成
    IndexBuffer(const std::vector<T>& indices, DXGI_FORMAT format)
    {
        auto device = DX12::Instance().getDevice();

        m_indexCount = static_cast<UINT>(indices.size());
        m_bufferSize = sizeof(T) * m_indexCount;

        CD3DX12_RESOURCE_DESC resourceDesc =
            CD3DX12_RESOURCE_DESC::Buffer(m_bufferSize);

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf())
        );
        LOG_HR(hr, "Failed to create index buffer.");

        //! Map → Copy → Unmap
        void* mapped = nullptr;
        hr = m_indexBuffer->Map(0, nullptr, &mapped);
        LOG_HR(hr, "Failed Map");

        memcpy(mapped, indices.data(), m_bufferSize);
        m_indexBuffer->Unmap(0, nullptr);

        //! IndexBufferView 作成
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = m_bufferSize;
        m_indexBufferView.Format = format;
    }

    //! インデックスバッファをバインド
    void bind() const
    {
        auto cmd = DX12::Instance().getGraphicsCommandList();
        cmd->IASetIndexBuffer(&m_indexBufferView);
    }

    //! インデックス数取得
    UINT getIndexCount() const
    {
        return m_indexCount;
    }

private:

    UINT m_indexCount = 0;
    UINT m_bufferSize = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
};