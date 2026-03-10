#pragma once

//=====================================================
// 定数バッファ管理クラス（テンプレート版）
//=====================================================
template<typename T>
class ConstantBuffer
{
public:

    ConstantBuffer(UINT elementCount = 1)
        : m_elementCount(elementCount)
    {
        m_elementSize = (sizeof(T) + 255) & ~255;
        m_bufferSize = m_elementSize * m_elementCount;

        m_uploadBuffer = std::make_unique<UploadBuffer>(m_bufferSize);

        auto res = m_uploadBuffer->getResource();
        res->Map(0, nullptr, reinterpret_cast<void**>(&m_mapped));

        // elementごとにCBV作成
        for (UINT i = 0; i < m_elementCount; ++i)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
            cbvDesc.BufferLocation = res->GetGPUVirtualAddress() + i * m_elementSize;
            cbvDesc.SizeInBytes = m_elementSize;

            m_cbvIndices.push_back(DescriptorHeapManager::Instance().createCBV(cbvDesc));
        }
    }

    //! コピー禁止
    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    //! ムーブ
    ConstantBuffer(ConstantBuffer&&) noexcept = default;
    ConstantBuffer& operator=(ConstantBuffer&&) noexcept = default;

    //! データ更新
    void update(const T& data, UINT index = 0)
    {
        memcpy(m_mapped + index * m_elementSize, &data, sizeof(T));
    }

    //! ルートパラメータ作成
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT index = 0) const
    {
        return DescriptorHeapManager::Instance().getGPUHandle(m_cbvIndices[index]);
    }

    //! GPU仮想アドレス取得
    D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress(UINT index = 0) const
    {
        return m_uploadBuffer->getResource()->GetGPUVirtualAddress() + index * m_elementSize;
    }

private:

    UINT m_elementCount = 0;
    UINT m_elementSize = 0;
    UINT m_bufferSize = 0;
    uint8_t* m_mapped = nullptr;
    std::unique_ptr<UploadBuffer> m_uploadBuffer;
    std::vector<UINT> m_cbvIndices;
};