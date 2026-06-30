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

        m_uploadBuffer = DXMem::makeUnique<UploadBuffer>(m_bufferSize);

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

    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    //! ムーブ
    ConstantBuffer(ConstantBuffer&& other) noexcept
        : m_elementCount(other.m_elementCount)
        , m_elementSize(other.m_elementSize)
        , m_bufferSize(other.m_bufferSize)
        , m_mapped(other.m_mapped)
        , m_uploadBuffer(std::move(other.m_uploadBuffer))
        , m_cbvIndices(std::move(other.m_cbvIndices))
    {
        other.m_elementCount = 0;
        other.m_elementSize = 0;
        other.m_bufferSize = 0;
        other.m_mapped = nullptr;
    }

    ConstantBuffer& operator=(ConstantBuffer&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            m_elementCount = other.m_elementCount;
            m_elementSize = other.m_elementSize;
            m_bufferSize = other.m_bufferSize;
            m_mapped = other.m_mapped;
            m_uploadBuffer = std::move(other.m_uploadBuffer);
            m_cbvIndices = std::move(other.m_cbvIndices);

            other.m_elementCount = 0;
            other.m_elementSize = 0;
            other.m_bufferSize = 0;
            other.m_mapped = nullptr;
        }
        return *this;
    }

    //! デストラクタ（CBV 解放と Unmap）
    ~ConstantBuffer()
    {
        cleanup();
    }

    //! データ更新
    void update(const T& data, UINT index = 0)
    {
        assert(index < m_elementCount && "ConstantBuffer::update out of bounds");
        if (m_mapped && index < m_elementCount)
        {
            memcpy(m_mapped + index * m_elementSize, &data, sizeof(T));
        }
    }

    //! ルートパラメータ作成
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT index = 0) const
    {
        assert(index < m_elementCount && "ConstantBuffer::getGPUHandle out of bounds");
        if (index >= m_elementCount) return {};
        return DescriptorHeapManager::Instance().getGPUHandle(m_cbvIndices[index]);
    }

    //! GPU仮想アドレス取得
    D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress(UINT index = 0) const
    {
        assert(index < m_elementCount && "ConstantBuffer::getGPUAddress out of bounds");
        if (index >= m_elementCount) return 0;
        return m_uploadBuffer->getResource()->GetGPUVirtualAddress() + index * m_elementSize;
    }

private:

    void cleanup()
    {
        // Unmap しておく
        if (m_uploadBuffer)
        {
            auto res = m_uploadBuffer->getResource();
            if (res && m_mapped)
            {
                res->Unmap(0, nullptr);
            }
        }

        // 確保した CBV を解放（無効インデックスはスキップ）
        for (UINT idx : m_cbvIndices)
        {
            if (idx != UINT_MAX)
            {
                DescriptorHeapManager::Instance().free(idx, 1);
            }
        }
        m_cbvIndices.clear();

        m_mapped = nullptr;
    }

    UINT m_elementCount = 0;
    UINT m_elementSize = 0;
    UINT m_bufferSize = 0;
    uint8_t* m_mapped = nullptr;
    std::unique_ptr<UploadBuffer> m_uploadBuffer;
    std::vector<UINT> m_cbvIndices;
};