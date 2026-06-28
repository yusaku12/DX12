#pragma once

//=====================================================
// プロファイラの CPU/GPU 時間計測
// FPS 計測
//=====================================================
class CpuGpuProfiler
{
public:

    struct GpuScopeToken
    {
        uint32_t id = 0;
        bool valid = false;
    };

    struct GpuScopeSample
    {
        std::string name;
        float gpuMs = 0.0f;
    };

    explicit CpuGpuProfiler();
    ~CpuGpuProfiler() {}

    //! フレーム開始
    void beginFrame(ID3D12GraphicsCommandList* cmd);

    //! フレーム終了
    void endFrame(ID3D12GraphicsCommandList* cmd);

    //! GPU スコープ計測開始
    GpuScopeToken beginGpuScope(ID3D12GraphicsCommandList* cmd, const char* name);

    //! GPU スコープ計測終了
    void endGpuScope(ID3D12GraphicsCommandList* cmd, GpuScopeToken token);

    //! CPU時間取得 (ms)
    void recordFps(float fps);

    //! CPU/GPU時間取得
    float getCpuTimems() const { return m_cpuTimeMs; }
    float getGpuTimems() const { return m_gpuTimeMs; }
    float getCpuMemoryMB() const { return m_cpuMemoryMB; }
    float getGpuMemoryMB() const { return m_gpuMemoryMB; }

    //! 履歴取得
    const std::vector<float>& cpuHistory() const { return m_cpuHistory; }
    const std::vector<float>& gpuHistory() const { return m_gpuHistory; }
    const std::vector<float>& fpsHistory() const { return m_fpsHistory; }
    const std::vector<float>& cpuMemoryHistory() const { return m_cpuMemoryHistory; }
    const std::vector<float>& gpuMemoryHistory() const { return m_gpuMemoryHistory; }
    const std::vector<GpuScopeSample>& gpuScopeSamples() const { return m_gpuScopeSamples; }

private:

    struct PendingScope
    {
        std::string name;
        UINT startQuery = UINT_MAX;
    };

    struct FinishedScope
    {
        std::string name;
        UINT startQuery = UINT_MAX;
        UINT endQuery = UINT_MAX;
    };

    struct QueryFrameData
    {
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer = nullptr;
        UINT queryCount = 0;
        UINT frameStartQuery = UINT_MAX;
        UINT frameEndQuery = UINT_MAX;
        std::unordered_map<uint32_t, PendingScope> pendingScopes;
        std::vector<FinishedScope> finishedScopes;
    };

    static constexpr UINT BUFFERED_FRAMES = 3;
    static constexpr UINT QUERY_COUNT_PER_FRAME = 2048;

    void pushHistory(std::vector<float>& history, float value);
    bool allocateQuery(QueryFrameData& frame, UINT& outQueryIndex);
    void consumeCompletedFrame();
    void sampleMemoryStats();
    void clearFrame(QueryFrameData& frame);

    //! CPU
    std::chrono::high_resolution_clock::time_point m_cpuStart;
    float m_cpuTimeMs = 0.0f;

    //! GPU
    std::array<QueryFrameData, BUFFERED_FRAMES> m_frames;
    uint32_t m_currentFrameIndex = 0;
    uint32_t m_lastCompletedFrameIndex = 0;
    bool m_hasCompletedFrame = false;
    std::mutex m_scopeMutex;
    uint32_t m_nextScopeToken = 1;

    Microsoft::WRL::ComPtr<IDXGIAdapter3> m_adapter3 = nullptr;
    UINT64 m_gpuFreq = 0;
    float m_gpuTimeMs = 0.0f;
    float m_cpuMemoryMB = 0.0f;
    float m_gpuMemoryMB = 0.0f;

    //! History buffers (Length 120 = 2秒分 @ 60FPS)
    std::vector<float> m_cpuHistory;
    std::vector<float> m_gpuHistory;
    std::vector<float> m_fpsHistory;
    std::vector<float> m_cpuMemoryHistory;
    std::vector<float> m_gpuMemoryHistory;
    std::vector<GpuScopeSample> m_gpuScopeSamples;
};