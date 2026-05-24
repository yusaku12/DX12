#include "pch.h"
#include "CommandListPool.h"

void CommandListPool::initialize(ID3D12Device* device, ID3D12Fence* fence, UINT poolSize)
{
    m_device = device;
    m_fence = fence;
    m_pool.resize(poolSize);

    for (auto& entry : m_pool)
    {
        HRESULT hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(entry.allocator.GetAddressOf()));
        LOG_HR(hr, "Failed to create worker CommandAllocator");

        hr = device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            entry.allocator.Get(),
            nullptr,
            IID_PPV_ARGS(entry.commandList.GetAddressOf()));
        LOG_HR(hr, "Failed to create worker CommandList");

        // 初期状態では Close しておく（acquire 時に Reset する）
        entry.commandList->Close();
        entry.inUse = false;
        entry.closed = true;
        entry.fenceValue = 0;
    }
}

ID3D12GraphicsCommandList* CommandListPool::acquire()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const UINT64 completedValue = m_fence ? m_fence->GetCompletedValue() : 0;

    // 高頻度なアロケーションでのO(1)再利用：
    // 前回使用したインデックスや、すでに完了済みのインサージョンを素早くチェックできるように最適化
    const size_t poolSize = m_pool.size();
    for (size_t i = 0; i < poolSize; ++i)
    {
        auto& entry = m_pool[i];
        if (entry.inUse && entry.fenceValue != 0 && entry.fenceValue <= completedValue)
        {
            entry.inUse = false;
            entry.fenceValue = 0;
        }

        if (!entry.inUse)
        {
            entry.allocator->Reset();
            entry.commandList->Reset(entry.allocator.Get(), nullptr);
            entry.inUse = true;
            entry.closed = false;
            entry.fenceValue = 0;
            return entry.commandList.Get();
        }
    }

    // プールが足りない場合は動的に追加
    CommandListEntry newEntry;
    HRESULT hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(newEntry.allocator.GetAddressOf()));
    LOG_HR(hr, "Failed to create dynamic worker CommandAllocator");

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        newEntry.allocator.Get(),
        nullptr,
        IID_PPV_ARGS(newEntry.commandList.GetAddressOf()));
    LOG_HR(hr, "Failed to create dynamic worker CommandList");

    newEntry.inUse = true;
    newEntry.closed = false;
    newEntry.fenceValue = 0;

    auto* ptr = newEntry.commandList.Get();
    m_pool.push_back(std::move(newEntry));
    return ptr;
}

void CommandListPool::release(ID3D12GraphicsCommandList* cmdList)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& entry : m_pool)
    {
        if (entry.commandList.Get() == cmdList)
        {
            if (!entry.closed)
            {
                cmdList->Close();
                entry.closed = true;
            }
            return;
        }
    }
}

std::vector<ID3D12CommandList*> CommandListPool::getClosedCommandLists() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ID3D12CommandList*> result;
    for (const auto& entry : m_pool)
    {
        if (entry.inUse && entry.closed)
        {
            result.push_back(entry.commandList.Get());
        }
    }
    return result;
}

void CommandListPool::notifySubmitted(UINT64 fenceValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& entry : m_pool)
    {
        if (entry.inUse && entry.closed && entry.fenceValue == 0)
        {
            entry.fenceValue = fenceValue;
        }
    }
}

void CommandListPool::resetCompleted()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_fence) return;

    const UINT64 completedValue = m_fence->GetCompletedValue();

    for (auto& entry : m_pool)
    {
        if (entry.inUse && entry.closed && entry.fenceValue != 0 && entry.fenceValue <= completedValue)
        {
            entry.inUse = false;
            entry.fenceValue = 0;
        }
    }
}

UINT CommandListPool::getActiveCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    UINT count = 0;
    for (const auto& entry : m_pool)
    {
        if (entry.inUse) ++count;
    }
    return count;
}