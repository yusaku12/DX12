#include "pch.h"

void DescriptorHeapManager::initialize(UINT maxCount)
{
    const auto device = DX12::Instance().getDevice();

    m_maxCount = maxCount;
    m_used.assign(maxCount, false);

    //! Descriptor Heap 作成
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = maxCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    m_incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DescriptorHeapManager::setDiscriptorHeap()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! ディスクリプタヒープを渡す
    ID3D12DescriptorHeap* heaps[] =
    {
        m_heap.Get()
    };
    cmd->SetDescriptorHeaps(_countof(heaps), heaps);
}

UINT DescriptorHeapManager::createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    UINT index = allocateRange();
    if (index == UINT_MAX) return UINT_MAX;
    device->CreateShaderResourceView(resource, &desc, getCPUHandle(index));
    return index;
}

UINT DescriptorHeapManager::createSRVArray(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, UINT arrayCount)
{
    const auto device = DX12::Instance().getDevice();

    UINT index = allocateRange(arrayCount);
    if (index == UINT_MAX) return UINT_MAX;

    auto cpuHandle = getCPUHandle(index);
    D3D12_SHADER_RESOURCE_VIEW_DESC arrayDesc = desc;

    for (UINT i = 0; i < arrayCount; i++)
    {
        device->CreateShaderResourceView(resource, &desc, cpuHandle);
        cpuHandle.ptr += m_incrementSize;
    }

    return index;
}

UINT DescriptorHeapManager::createCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    UINT index = allocateRange();
    if (index == UINT_MAX) return UINT_MAX;
    device->CreateConstantBufferView(&desc, getCPUHandle(index));
    return index;
}

UINT DescriptorHeapManager::createUAV(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    UINT index = allocateRange();
    if (index == UINT_MAX) return UINT_MAX;
    device->CreateUnorderedAccessView(resource, counterResource, &desc, getCPUHandle(index));
    return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getGPUHandle(UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = m_heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += index * m_incrementSize;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCPUHandle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += index * m_incrementSize;
    return h;
}

void DescriptorHeapManager::free(UINT index, UINT count)
{
    if (index == UINT_MAX) return;

    for (UINT i = 0; i < count && index + i < m_maxCount; i++)
    {
        m_used[index + i] = false;
    }
}

UINT DescriptorHeapManager::allocateRange(UINT count)
{
    if (count == 0) return UINT_MAX;

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