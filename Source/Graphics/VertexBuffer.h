#pragma once

//=====================================================
// バーテックスデータ作成クラス（テンプレート版）
//=====================================================
template<typename T>
class VertexBuffer
{
public:

    //! コンストラクタで頂点バッファ生成
    VertexBuffer(const std::vector<T>& vertices)
    {
        auto device = DX12::Instance().getDevice();

        m_vertexCount = static_cast<UINT>(vertices.size());
        m_bufferSize = sizeof(T) * m_vertexCount;

        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_bufferSize);

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) return;

        //! Map → Copy → Unmap
        void* mapped = nullptr;
        hr = m_vertexBuffer->Map(0, nullptr, &mapped);
        if (FAILED(hr)) return;

        memcpy(mapped, vertices.data(), m_bufferSize);
        m_vertexBuffer->Unmap(0, nullptr);

        //! VertexBufferView 作成
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = m_bufferSize;
        m_vertexBufferView.StrideInBytes = sizeof(T);
    }

    //! バーテックスバッファをバインド
    void bind() const
    {
        auto cmd = DX12::Instance().getGraphicsCommandList();
        cmd->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    }

    //! 頂点数取得
    UINT getVertexCount() const
    {
        return m_vertexCount;
    }

private:

    UINT m_vertexCount = 0;
    UINT m_bufferSize = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
};