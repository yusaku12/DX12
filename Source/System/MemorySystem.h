#pragma once

//=====================================================
// メモリシステム
//=====================================================
class MemorySystem
{
public:

    //=====================================================
    // メモリリーク統計
    //=====================================================
    struct LeakStats
    {
        uint64_t activeAllocations = 0;
        uint64_t activeBytes = 0;
        uint64_t peakActiveBytes = 0;
        uint64_t totalAllocations = 0;
        uint64_t totalFrees = 0;
    };

    //=====================================================
    // アロケータ統計
    //=====================================================
    struct LinearAllocatorStats
    {
        size_t capacityBytes = 0;
        size_t usedBytes = 0;
        size_t peakBytes = 0;
    };

    //=====================================================
    // スタックアロケータ統計
    //=====================================================
    struct StackAllocatorStats
    {
        size_t capacityBytes = 0;
        size_t usedBytes = 0;
        size_t peakBytes = 0;
    };

    //=====================================================
    // プールアロケータ統計
    //=====================================================
    struct PoolAllocatorStats
    {
        size_t blockSizeBytes = 0;
        size_t capacityBlocks = 0;
        size_t activeBlocks = 0;
        size_t peakActiveBlocks = 0;
    };

    static MemorySystem& Instance()
    {
        static MemorySystem instance;
        return instance;
    }

    // 初期化
    void initialize();

    // 終了処理
    void shutdown();

    // フレーム開始時に呼び出す
    void beginFrame();

    // デバイスをバインドする（GPU メモリ統計の取得に使用）
    void bindDevice(ID3D12Device* device);

    // アロケーション関数
    void* linearAllocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // アロケーション関数（スタックアロケータ）
    void* stackAllocate(size_t size, size_t alignment = alignof(std::max_align_t));

    // アロケーション関数（プールアロケータ）
    void stackDeallocate(void* ptr);

    // アロケーション関数（プールアロケータ）
    void* poolAllocate();

    // アロケーション関数（プールアロケータ）
    void poolDeallocate(void* ptr);

    //
    LeakStats getLeakStats() const;

    //
    LinearAllocatorStats getLinearAllocatorStats() const;

    //
    StackAllocatorStats getStackAllocatorStats() const;

    //
    PoolAllocatorStats getPoolAllocatorStats() const;

    //
    float getGpuLocalUsageMB() const { return m_gpuLocalUsageMB; }

    //
    float getGpuLocalBudgetMB() const { return m_gpuLocalBudgetMB; }

    //
    float getGpuLocalUsageRatio() const { return m_gpuLocalUsageRatio; }

    //
    float getGpuLocalPeakMB() const { return m_gpuLocalPeakMB; }

    //
    template<typename T, typename... Args>
    T* newTracked(const char* file, int line, const char* tag, Args&&... args)
    {
        void* mem = ::operator new(sizeof(T), std::nothrow);
        if (!mem)
        {
            LOG_ERROR("MemorySystem: allocation failed (%zu bytes)", sizeof(T));
            return nullptr;
        }

        trackAllocation(mem, sizeof(T), file, line, tag);
        return new (mem) T(std::forward<Args>(args)...);
    }

    //
    template<typename T>
    void deleteTracked(T* ptr)
    {
        if (!ptr)
        {
            return;
        }

        ptr->~T();
        untrackAllocation(ptr);
        ::operator delete(ptr);
    }

    //
    template<typename T, typename... Args>
    std::shared_ptr<T> makeSharedTracked(const char* file, int line, const char* tag, Args&&... args)
    {
        T* raw = newTracked<T>(file, line, tag, std::forward<Args>(args)...);
        if (!raw)
        {
            return {};
        }

        return std::shared_ptr<T>(raw, [](T* p)
            {
                MemorySystem::Instance().deleteTracked<T>(p);
            });
    }

private:

    struct LeakRecord
    {
        size_t sizeBytes = 0;
        std::string file;
        int line = 0;
        std::string tag;
    };

    class LinearAllocator
    {
    public:

        //
        bool initialize(size_t capacityBytes);

        //
        void shutdown();

        //
        void* allocate(size_t sizeBytes, size_t alignment);

        //
        void reset();

        //
        LinearAllocatorStats getStats() const;

    private:

        std::vector<std::byte> m_buffer;
        size_t m_offset = 0;
        size_t m_peak = 0;
        mutable std::mutex m_mutex;
    };

    class StackAllocator
    {
    public:

        //
        bool initialize(size_t capacityBytes);

        //
        void shutdown();

        //
        void* allocate(size_t sizeBytes, size_t alignment);

        //
        void deallocate(void* ptr);

        //
        void reset();

        //
        StackAllocatorStats getStats() const;

    private:

        struct Header
        {
            size_t previousOffset = 0;
        };

        std::vector<std::byte> m_buffer;
        size_t m_offset = 0;
        size_t m_peak = 0;
        mutable std::mutex m_mutex;
    };

    class PoolAllocator
    {
    public:

        bool initialize(size_t blockSizeBytes, size_t blockCount);

        void shutdown();

        void* allocate();

        void deallocate(void* ptr);

        PoolAllocatorStats getStats() const;

    private:

        bool owns(const void* ptr) const;

        std::vector<std::byte> m_buffer;
        void* m_freeHead = nullptr;
        size_t m_blockSize = 0;
        size_t m_blockCount = 0;
        size_t m_activeBlocks = 0;
        size_t m_peakActiveBlocks = 0;
        mutable std::mutex m_mutex;
    };

    MemorySystem() = default;
    ~MemorySystem() = default;
    MemorySystem(const MemorySystem&) = delete;
    MemorySystem& operator=(const MemorySystem&) = delete;

    void trackAllocation(void* ptr, size_t sizeBytes, const char* file, int line, const char* tag);
    void untrackAllocation(void* ptr);
    void dumpLeaks() const;
    void sampleGpuMemory();

    bool m_initialized = false;
    mutable std::mutex m_leakMutex;
    std::unordered_map<void*, LeakRecord> m_activeLeaks;
    LeakStats m_leakStats{};

    LinearAllocator m_linearAllocator;
    StackAllocator m_stackAllocator;
    PoolAllocator m_poolAllocator;

    Microsoft::WRL::ComPtr<IDXGIAdapter3> m_adapter3 = nullptr;
    float m_gpuLocalUsageMB = 0.0f;
    float m_gpuLocalBudgetMB = 0.0f;
    float m_gpuLocalUsageRatio = 0.0f;
    float m_gpuLocalPeakMB = 0.0f;
};

#define DX_NEW(Type, ...) MemorySystem::Instance().newTracked<Type>(__FILE__, __LINE__, #Type, ##__VA_ARGS__)
#define DX_DELETE(ptr) MemorySystem::Instance().deleteTracked(ptr)
#define DX_MAKE_SHARED(Type, ...) MemorySystem::Instance().makeSharedTracked<Type>(__FILE__, __LINE__, #Type, ##__VA_ARGS__)
#define DX_UNIQUE_PTR(Type, ...) DXMem::makeUnique<Type>(__VA_ARGS__)

namespace DXMem
{
    template<typename T, typename... Args>
    std::unique_ptr<T> makeUnique(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
}
