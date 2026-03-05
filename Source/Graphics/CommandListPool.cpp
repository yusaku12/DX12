#include "pch.h"
#include "CommandListPool.h"

void CommandListPool::initialize(ID3D12Device* device, UINT poolSize)
{
    m_device = device;
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

        //! 初期状態では Close しておく（acquire 時に Reset する）
        entry.commandList->Close();
        entry.inUse = false;
        entry.closed = true;
    }
}

ID3D12GraphicsCommandList* CommandListPool::acquire()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& entry : m_pool)
    {
        if (!entry.inUse)
        {
            entry.allocator->Reset();
            entry.commandList->Reset(entry.allocator.Get(), nullptr);
            entry.inUse = true;
            entry.closed = false;
            return entry.commandList.Get();
        }
    }

    //! プールが足りない場合は動的に追加
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

void CommandListPool::resetAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& entry : m_pool)
    {
        if (entry.inUse && !entry.closed)
        {
            entry.commandList->Close();
            entry.closed = true;
        }
        entry.inUse = false;
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