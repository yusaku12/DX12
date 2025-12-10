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

UINT DescriptorHeapManager::createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    UINT index = allocate();
    auto cpuHandle = getCPUHandle(index);
    device->CreateShaderResourceView(resource, &desc, cpuHandle);
    return index;
}

UINT DescriptorHeapManager::createCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    UINT index = allocate();
    auto cpuHandle = getCPUHandle(index);
    device->CreateConstantBufferView(&desc, cpuHandle);
    return index;
}

UINT DescriptorHeapManager::createUAV(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    UINT index = allocate();
    auto cpuHandle = getCPUHandle(index);
    device->CreateUnorderedAccessView(resource, counterResource, &desc, cpuHandle);
    return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getGPUHandle(UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = m_heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += index * m_incrementSize;
    return h;
}

void DescriptorHeapManager::free(UINT index)
{
    if (index < m_maxCount) m_used[index] = false;
}

UINT DescriptorHeapManager::allocate()
{
    for (UINT i = 0; i < m_maxCount; i++)
    {
        if (!m_used[i])
        {
            m_used[i] = true;
            return i;
        }
    }
    return UINT_MAX; // 空きがない
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCPUHandle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += index * m_incrementSize;
    return h;
}