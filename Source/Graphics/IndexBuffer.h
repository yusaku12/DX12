#pragma once

//=====================================================
// インデックスバッファ作成クラス（テンプレート版）
//=====================================================
template<typename T>
class IndexBuffer
{
public:

    //! コンストラクタでインデックスバッファ生成（std::vector版）
    IndexBuffer(const std::vector<T>& indices)
        : IndexBuffer(indices.data(), static_cast<UINT>(indices.size()))
    {
    }

    //! コンストラクタでインデックスバッファ生成（配列版）
    template<size_t N>
    IndexBuffer(const T(&indices)[N])
        : IndexBuffer(indices, static_cast<UINT>(N))
    {
    }

    //! コンストラクタでインデックスバッファ生成
    IndexBuffer(const T* indices, UINT count)
    {
        m_indexCount = count;
        m_bufferSize = sizeof(T) * m_indexCount;

        //! UploadBuffer 作成
        m_uploadBuffer = std::make_unique<UploadBuffer>(m_bufferSize);

        void* mapped = nullptr;
        HRESULT hr = m_uploadBuffer->getResource()->Map(0, nullptr, &mapped);
        LOG_HR(hr, "Failed Map");

        memcpy(mapped, indices, m_bufferSize);
        m_uploadBuffer->getResource()->Unmap(0, nullptr);

        //! Format 自動判定
        DXGI_FORMAT format = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

        //! IB View
        m_indexBufferView.BufferLocation = m_uploadBuffer->getResource()->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = m_bufferSize;
        m_indexBufferView.Format = format;
    }

    //! インデックスバッファをバインド
    void bind() const
    {
        auto cmd = DX12::Instance().getGraphicsCommandList();
        cmd->IASetIndexBuffer(&m_indexBufferView);
    }

    //! インデックスバッファをバインド（指定コマンドリスト）
    void bind(ID3D12GraphicsCommandList* cmd) const
    {
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
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    std::unique_ptr<UploadBuffer>m_uploadBuffer;
};