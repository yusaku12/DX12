#include "pch.h"
#include "MemorySystem.h"

namespace
{
    constexpr size_t kLinearAllocatorSize = 16ull * 1024ull * 1024ull;
    constexpr size_t kStackAllocatorSize = 8ull * 1024ull * 1024ull;
    constexpr size_t kPoolBlockSize = 256ull;
    constexpr size_t kPoolBlockCount = 4096ull;

    size_t alignUp(size_t value, size_t alignment)
    {
        if (alignment <= 1)
        {
            return value;
        }

        const size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }
}

void MemorySystem::initialize()
{
    if (m_initialized)
    {
        return;
    }

    const bool linearOk = m_linearAllocator.initialize(kLinearAllocatorSize);
    const bool stackOk = m_stackAllocator.initialize(kStackAllocatorSize);
    const bool poolOk = m_poolAllocator.initialize(kPoolBlockSize, kPoolBlockCount);

    if (!linearOk || !stackOk || !poolOk)
    {
        LOG_ERROR("MemorySystem: allocator initialize failed (linear=%d stack=%d pool=%d)",
            linearOk ? 1 : 0,
            stackOk ? 1 : 0,
            poolOk ? 1 : 0);
        return;
    }

    LOG_ASSERT(linearOk, "MemorySystem: LinearAllocator initialize failed");
    LOG_ASSERT(stackOk, "MemorySystem: StackAllocator initialize failed");
    LOG_ASSERT(poolOk, "MemorySystem: PoolAllocator initialize failed");

    m_leakStats = {};
    m_gpuLocalUsageMB = 0.0f;
    m_gpuLocalBudgetMB = 0.0f;
    m_gpuLocalUsageRatio = 0.0f;
    m_gpuLocalPeakMB = 0.0f;
    m_initialized = true;

    LOG_INFO("MemorySystem initialized (Linear=%zuMB Stack=%zuMB Pool=%zu x %zuB)",
        kLinearAllocatorSize / (1024 * 1024),
        kStackAllocatorSize / (1024 * 1024),
        kPoolBlockCount,
        kPoolBlockSize);
}

void MemorySystem::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    dumpLeaks();

    m_poolAllocator.shutdown();
    m_stackAllocator.shutdown();
    m_linearAllocator.shutdown();

    m_adapter3.Reset();
    m_initialized = false;
    LOG_INFO("MemorySystem shutdown completed");
}

void MemorySystem::beginFrame()
{
    if (!m_initialized)
    {
        return;
    }

    m_linearAllocator.reset();
    sampleGpuMemory();
}

void MemorySystem::bindDevice(ID3D12Device* device)
{
    m_adapter3.Reset();

    if (!device)
    {
        return;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
    {
        return;
    }

    const LUID targetLuid = device->GetAdapterLuid();
    for (UINT i = 0;; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        if (FAILED(adapter->GetDesc1(&desc)))
        {
            continue;
        }

        if (std::memcmp(&desc.AdapterLuid, &targetLuid, sizeof(LUID)) == 0)
        {
            adapter.As(&m_adapter3);
            break;
        }
    }

    sampleGpuMemory();
}

void* MemorySystem::linearAllocate(size_t size, size_t alignment)
{
    return m_linearAllocator.allocate(size, alignment);
}

void* MemorySystem::stackAllocate(size_t size, size_t alignment)
{
    return m_stackAllocator.allocate(size, alignment);
}

void MemorySystem::stackDeallocate(void* ptr)
{
    m_stackAllocator.deallocate(ptr);
}

void* MemorySystem::poolAllocate()
{
    return m_poolAllocator.allocate();
}

void MemorySystem::poolDeallocate(void* ptr)
{
    m_poolAllocator.deallocate(ptr);
}

MemorySystem::LeakStats MemorySystem::getLeakStats() const
{
    std::lock_guard<std::mutex> lock(m_leakMutex);
    return m_leakStats;
}

MemorySystem::LinearAllocatorStats MemorySystem::getLinearAllocatorStats() const
{
    return m_linearAllocator.getStats();
}

MemorySystem::StackAllocatorStats MemorySystem::getStackAllocatorStats() const
{
    return m_stackAllocator.getStats();
}

MemorySystem::PoolAllocatorStats MemorySystem::getPoolAllocatorStats() const
{
    return m_poolAllocator.getStats();
}

void MemorySystem::trackAllocation(void* ptr, size_t sizeBytes, const char* file, int line, const char* tag)
{
    if (!ptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_leakMutex);

    LeakRecord record;
    record.sizeBytes = sizeBytes;
    record.file = file ? file : "";
    record.line = line;
    record.tag = tag ? tag : "";

    m_activeLeaks[ptr] = std::move(record);

    m_leakStats.activeAllocations = static_cast<uint64_t>(m_activeLeaks.size());
    m_leakStats.activeBytes += static_cast<uint64_t>(sizeBytes);
    m_leakStats.totalAllocations++;
    m_leakStats.peakActiveBytes = std::max(m_leakStats.peakActiveBytes, m_leakStats.activeBytes);
}

void MemorySystem::untrackAllocation(void* ptr)
{
    if (!ptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_leakMutex);

    auto it = m_activeLeaks.find(ptr);
    if (it == m_activeLeaks.end())
    {
        return;
    }

    m_leakStats.activeBytes -= static_cast<uint64_t>(it->second.sizeBytes);
    m_activeLeaks.erase(it);

    m_leakStats.activeAllocations = static_cast<uint64_t>(m_activeLeaks.size());
    m_leakStats.totalFrees++;
}

void MemorySystem::dumpLeaks() const
{
    std::lock_guard<std::mutex> lock(m_leakMutex);

    if (m_activeLeaks.empty())
    {
        LOG_INFO("MemorySystem: no tracked leaks (peak=%.2f MB)", static_cast<float>(m_leakStats.peakActiveBytes) / (1024.0f * 1024.0f));
        return;
    }

    LOG_ERROR("MemorySystem: detected %llu leaks (%.2f MB active)",
        static_cast<unsigned long long>(m_leakStats.activeAllocations),
        static_cast<float>(m_leakStats.activeBytes) / (1024.0f * 1024.0f));

    std::vector<std::pair<void*, LeakRecord>> entries;
    entries.reserve(m_activeLeaks.size());
    for (const auto& kv : m_activeLeaks)
    {
        entries.push_back(kv);
    }

    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b)
        {
            return a.second.sizeBytes > b.second.sizeBytes;
        });

    const size_t maxDump = std::min<size_t>(entries.size(), 128);
    for (size_t i = 0; i < maxDump; ++i)
    {
        const auto& e = entries[i];
        LOG_ERROR("  leak[%zu] ptr=%p size=%zu tag=%s (%s:%d)",
            i,
            e.first,
            e.second.sizeBytes,
            e.second.tag.c_str(),
            e.second.file.c_str(),
            e.second.line);
    }
}

void MemorySystem::sampleGpuMemory()
{
    if (!m_adapter3)
    {
        return;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
    if (FAILED(m_adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
    {
        return;
    }

    m_gpuLocalUsageMB = static_cast<float>(info.CurrentUsage) / (1024.0f * 1024.0f);
    m_gpuLocalBudgetMB = static_cast<float>(info.Budget) / (1024.0f * 1024.0f);
    m_gpuLocalPeakMB = std::max(m_gpuLocalPeakMB, m_gpuLocalUsageMB);

    if (m_gpuLocalBudgetMB > 0.0f)
    {
        m_gpuLocalUsageRatio = m_gpuLocalUsageMB / m_gpuLocalBudgetMB;
    }
    else
    {
        m_gpuLocalUsageRatio = 0.0f;
    }
}

bool MemorySystem::LinearAllocator::initialize(size_t capacityBytes)
{
    m_buffer.clear();
    m_buffer.resize(capacityBytes);
    m_offset = 0;
    m_peak = 0;
    return !m_buffer.empty();
}

void MemorySystem::LinearAllocator::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.clear();
    m_offset = 0;
    m_peak = 0;
}

void* MemorySystem::LinearAllocator::allocate(size_t sizeBytes, size_t alignment)
{
    if (sizeBytes == 0)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty())
    {
        return nullptr;
    }

    const size_t aligned = alignUp(m_offset, alignment);
    const size_t end = aligned + sizeBytes;

    if (end > m_buffer.size())
    {
        return nullptr;
    }

    void* ptr = m_buffer.data() + aligned;
    m_offset = end;
    m_peak = std::max(m_peak, m_offset);
    return ptr;
}

void MemorySystem::LinearAllocator::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_offset = 0;
}

MemorySystem::LinearAllocatorStats MemorySystem::LinearAllocator::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    LinearAllocatorStats stats;
    stats.capacityBytes = m_buffer.size();
    stats.usedBytes = m_offset;
    stats.peakBytes = m_peak;
    return stats;
}

bool MemorySystem::StackAllocator::initialize(size_t capacityBytes)
{
    m_buffer.clear();
    m_buffer.resize(capacityBytes);
    m_offset = 0;
    m_peak = 0;
    return !m_buffer.empty();
}

void MemorySystem::StackAllocator::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.clear();
    m_offset = 0;
    m_peak = 0;
}

void* MemorySystem::StackAllocator::allocate(size_t sizeBytes, size_t alignment)
{
    if (sizeBytes == 0)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty())
    {
        return nullptr;
    }

    const size_t aligned = alignUp(m_offset + sizeof(Header), alignment);
    const size_t end = aligned + sizeBytes;
    if (end > m_buffer.size())
    {
        return nullptr;
    }

    auto* header = reinterpret_cast<Header*>(m_buffer.data() + aligned - sizeof(Header));
    header->previousOffset = m_offset;

    void* ptr = m_buffer.data() + aligned;
    m_offset = end;
    m_peak = std::max(m_peak, m_offset);
    return ptr;
}

void MemorySystem::StackAllocator::deallocate(void* ptr)
{
    if (!ptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty())
    {
        return;
    }

    const std::byte* begin = m_buffer.data();
    const std::byte* end = begin + m_buffer.size();
    const std::byte* target = reinterpret_cast<std::byte*>(ptr);

    if (target <= begin || target > end)
    {
        return;
    }

    const auto* header = reinterpret_cast<const Header*>(target - sizeof(Header));
    m_offset = header->previousOffset;
}

void MemorySystem::StackAllocator::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_offset = 0;
}

MemorySystem::StackAllocatorStats MemorySystem::StackAllocator::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    StackAllocatorStats stats;
    stats.capacityBytes = m_buffer.size();
    stats.usedBytes = m_offset;
    stats.peakBytes = m_peak;
    return stats;
}

bool MemorySystem::PoolAllocator::initialize(size_t blockSizeBytes, size_t blockCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const size_t actualBlockSize = std::max(blockSizeBytes, sizeof(void*));
    if (actualBlockSize == 0 || blockCount == 0)
    {
        return false;
    }

    m_blockSize = actualBlockSize;
    m_blockCount = blockCount;
    m_buffer.clear();
    m_buffer.resize(m_blockSize * m_blockCount);

    m_freeHead = nullptr;
    m_activeBlocks = 0;
    m_peakActiveBlocks = 0;

    for (size_t i = 0; i < m_blockCount; ++i)
    {
        void* block = m_buffer.data() + i * m_blockSize;
        *reinterpret_cast<void**>(block) = m_freeHead;
        m_freeHead = block;
    }

    return true;
}

void MemorySystem::PoolAllocator::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.clear();
    m_freeHead = nullptr;
    m_blockSize = 0;
    m_blockCount = 0;
    m_activeBlocks = 0;
    m_peakActiveBlocks = 0;
}

void* MemorySystem::PoolAllocator::allocate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_freeHead)
    {
        return nullptr;
    }

    void* block = m_freeHead;
    m_freeHead = *reinterpret_cast<void**>(m_freeHead);
    ++m_activeBlocks;
    m_peakActiveBlocks = std::max(m_peakActiveBlocks, m_activeBlocks);
    return block;
}

void MemorySystem::PoolAllocator::deallocate(void* ptr)
{
    if (!ptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!owns(ptr))
    {
        return;
    }

    *reinterpret_cast<void**>(ptr) = m_freeHead;
    m_freeHead = ptr;
    if (m_activeBlocks > 0)
    {
        --m_activeBlocks;
    }
}

MemorySystem::PoolAllocatorStats MemorySystem::PoolAllocator::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    PoolAllocatorStats stats;
    stats.blockSizeBytes = m_blockSize;
    stats.capacityBlocks = m_blockCount;
    stats.activeBlocks = m_activeBlocks;
    stats.peakActiveBlocks = m_peakActiveBlocks;
    return stats;
}

bool MemorySystem::PoolAllocator::owns(const void* ptr) const
{
    if (m_buffer.empty())
    {
        return false;
    }

    const auto* begin = reinterpret_cast<const std::byte*>(m_buffer.data());
    const auto* end = begin + m_buffer.size();
    const auto* target = reinterpret_cast<const std::byte*>(ptr);

    if (target < begin || target >= end)
    {
        return false;
    }

    const ptrdiff_t offset = target - begin;
    return (offset % static_cast<ptrdiff_t>(m_blockSize)) == 0;
}
