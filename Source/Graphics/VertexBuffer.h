#pragma once

//=====================================================
// バーテックスデータ作成クラス（テンプレート版）
//=====================================================
template<typename T>
class VertexBuffer
{
public:

    //! コンストラクタで頂点バッファ生成（std::vector版）
    VertexBuffer(const std::vector<T>& vertices)
        : VertexBuffer(vertices.data(), static_cast<UINT>(vertices.size()))
    {
    }

    //! コンストラクタで頂点バッファ生成（配列版）
    template<size_t N>
    VertexBuffer(const T(&vertices)[N])
        : VertexBuffer(vertices, static_cast<UINT>(N))
    {
    }

    //! コンストラクタで頂点バッファ生成
    VertexBuffer(const T* vertices, UINT count)
    {
        m_vertexCount = count;
        m_bufferSize = sizeof(T) * m_vertexCount;

        //! バッファ作成
        m_uploadBuffer = std::make_unique<UploadBuffer>(m_bufferSize);

        //! Map → Copy → Unmap
        void* mapped = nullptr;
        HRESULT hr = m_uploadBuffer->getResource()->Map(0, nullptr, &mapped);
        LOG_HR(hr, "failed Map");

        memcpy(mapped, vertices, m_bufferSize);
        m_uploadBuffer->getResource()->Unmap(0, nullptr);

        //! VertexBufferView 作成
        m_vertexBufferView.BufferLocation = m_uploadBuffer->getResource()->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = m_bufferSize;
        m_vertexBufferView.StrideInBytes = sizeof(T);
    }

    //! バーテックスバッファをバインド
    void bind() const
    {
        auto cmd = DX12::Instance().getGraphicsCommandList();
        cmd->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    }

    //! バーテックスバッファをバインド（指定コマンドリスト）
    void bind(ID3D12GraphicsCommandList* cmd) const
    {
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
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    std::unique_ptr<UploadBuffer>m_uploadBuffer;
};