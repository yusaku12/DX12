#include "pch.h"

void DescriptorHeapManager::initialize(UINT maxCount)
{
    const auto device = DX12::Instance().getDevice();

    m_maxCount = maxCount;
    m_used.assign(maxCount, false);

    //! shader-visible Descriptor Heap 作成（既存）
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = maxCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    LOG_HR(hr, "DescriptorHeap CreateDescriptorHeap failed");
    m_incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //! CPU-only Descriptor Heap 作成（コピー元として使う）
    D3D12_DESCRIPTOR_HEAP_DESC cpuDesc = {};
    cpuDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cpuDesc.NumDescriptors = maxCount;
    cpuDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU-only
    hr = device->CreateDescriptorHeap(&cpuDesc, IID_PPV_ARGS(&m_cpuHeap));
    LOG_HR(hr, "DescriptorHeap CreateDescriptorHeap (CPU-only) failed");

    // 既存コードの互換性を保つため、インデックス0を予約している既存実装の挙動を尊重
    if (maxCount > 0)
        m_used[0] = true;
}

void DescriptorHeapManager::setDescriptorHeap()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    ID3D12DescriptorHeap* heaps[] = { m_heap.Get() };
    cmd->SetDescriptorHeaps(_countof(heaps), heaps);
}

UINT DescriptorHeapManager::createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    if (!device || !resource) return InvalidIndex;

    UINT index = allocateRange();
    if (index == InvalidIndex) return InvalidIndex;

    // まず CPU-only ヒープに書く（読み取り可能なソースを作る）
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = getCPUHandleCpuHeap(index);
    device->CreateShaderResourceView(resource, &desc, cpuHandle);

    // CPU-only から shader-visible ヒープへコピー
    D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = getCPUHandle(index); // shader-visible の CPU ハンドル
    device->CopyDescriptorsSimple(
        1,
        dstHandle,
        cpuHandle,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    );

    return index;
}

UINT DescriptorHeapManager::createSRVArray(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, UINT arrayCount)
{
    const auto device = DX12::Instance().getDevice();
    if (!device || !resource || arrayCount == 0) return InvalidIndex;

    UINT index = allocateRange(arrayCount);
    if (index == InvalidIndex) return InvalidIndex;

    for (UINT i = 0; i < arrayCount; i++)
    {
        // CPU-only ヒープへ作成
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = getCPUHandleCpuHeap(index + i);
        device->CreateShaderResourceView(resource, &desc, cpuHandle);

        // コピーして shader-visible ヒープに配置
        D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = getCPUHandle(index + i);
        device->CopyDescriptorsSimple(
            1,
            dstHandle,
            cpuHandle,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );
    }

    return index;
}

UINT DescriptorHeapManager::createCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    if (!device) return InvalidIndex;

    UINT index = allocateRange();
    if (index == InvalidIndex) return InvalidIndex;
    device->CreateConstantBufferView(&desc, getCPUHandle(index));
    return index;
}

UINT DescriptorHeapManager::createUAV(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    if (!device || !resource) return InvalidIndex;

    UINT index = allocateRange();
    if (index == InvalidIndex) return InvalidIndex;
    device->CreateUnorderedAccessView(resource, counterResource, &desc, getCPUHandle(index));
    return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getGPUHandle(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = {};
    if (index == InvalidIndex || index >= m_maxCount || !m_heap)
        return h;

    h = m_heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * m_incrementSize;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCPUHandle(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = {};
    if (index == InvalidIndex || index >= m_maxCount || !m_heap)
        return h;

    h = m_heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * m_incrementSize;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCPUHandleCpuHeap(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = {};
    if (index == InvalidIndex || index >= m_maxCount || !m_cpuHeap)
        return h;

    h = m_cpuHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * m_incrementSize;
    return h;
}

void DescriptorHeapManager::free(UINT index, UINT count)
{
    if (index == InvalidIndex || count == 0) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (index >= m_maxCount) return;
    for (UINT i = 0; i < count && (index + i) < m_maxCount; ++i)
    {
        m_used[index + i] = false;
    }
}

UINT DescriptorHeapManager::allocateRange(UINT count)
{
    if (count == 0) return InvalidIndex;
    std::lock_guard<std::mutex> lock(m_mutex);

    //! 既存実装はインデックス1から探索している挙動に合わせる
    for (UINT start = 1; start + count <= m_maxCount; ++start)
    {
        bool freeBlock = true;
        for (UINT offset = 0; offset < count; ++offset)
        {
            if (m_used[start + offset])
            {
                freeBlock = false;
                break;
            }
        }

        if (freeBlock)
        {
            for (UINT offset = 0; offset < count; ++offset)
            {
                m_used[start + offset] = true;
            }
            return start;
        }
    }

    return UINT_MAX;
}

bool DescriptorHeapManager::copyDescriptorsRange(UINT dstIndex, const std::vector<UINT>& srcIndices)
{
    if (dstIndex == InvalidIndex || srcIndices.empty()) return false;

    const auto device = DX12::Instance().getDevice();
    if (!device) return false;

    // dstIndex + i must be within heap range
    for (size_t i = 0; i < srcIndices.size(); ++i)
    {
        UINT src = srcIndices[i];
        UINT dst = dstIndex + static_cast<UINT>(i);
        if (src == InvalidIndex || dst >= m_maxCount) return false;

        // Copy from CPU-only heap (読み取り可能なソース)
        D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = getCPUHandleCpuHeap(src);
        D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = getCPUHandle(dst);

        device->CopyDescriptorsSimple(
            1,
            dstHandle,
            srcHandle,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );
    }

    return true;
}