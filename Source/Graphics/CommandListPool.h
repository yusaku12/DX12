#pragma once

#include <vector>
#include <mutex>
#include <d3d12.h>
#include <wrl/client.h>

class CommandListPool
{
public:

    struct CommandListEntry
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
        bool inUse = false;
        bool closed = false;
    };

    static CommandListPool& Instance()
    {
        static CommandListPool instance;
        return instance;
    }

    void initialize(ID3D12Device* device, UINT poolSize = 4);
    ID3D12GraphicsCommandList* acquire();
    void release(ID3D12GraphicsCommandList* cmdList);
    std::vector<ID3D12CommandList*> getClosedCommandLists() const;
    void resetAll();
    UINT getActiveCount() const;

private:

    CommandListPool() = default;
    ~CommandListPool() = default;
    CommandListPool(const CommandListPool&) = delete;
    CommandListPool& operator=(const CommandListPool&) = delete;

    ID3D12Device* m_device = nullptr;
    std::vector<CommandListEntry> m_pool;
    mutable std::mutex m_mutex;
};
