#pragma once

//=====================================================
// CpuGpuProfiler クラス
//=====================================================
class CpuGpuProfiler
{
public:

    explicit CpuGpuProfiler();
    ~CpuGpuProfiler() {}

    //! フレーム開始
    void beginFrame(ID3D12GraphicsCommandList* cmd);

    //! フレーム終了
    void endFrame(ID3D12GraphicsCommandList* cmd);

    //! CPU時間取得 (ms)
    void recordFps(float fps);

    //! CPU/GPU時間取得
    float getCpuTimems() const { return m_cpuTimeMs; }
    float getGpuTimems() const { return m_gpuTimeMs; }

    //! 履歴取得
    const std::vector<float>& cpuHistory() const { return m_cpuHistory; }
    const std::vector<float>& gpuHistory() const { return m_gpuHistory; }
    const std::vector<float>& fpsHistory() const { return m_fpsHistory; }

private:

    //! CPU
    std::chrono::high_resolution_clock::time_point m_cpuStart;
    float m_cpuTimeMs = 0.0f;

    //! GPU
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_queryBuffer = nullptr;

    static constexpr int QUERYCOUNT = 2; //!< Start/End
    UINT64 m_gpuFreq = 0;
    float m_gpuTimeMs = 0.0f;

    //! History buffers (Length 120 = 2秒分 @ 60FPS)
    std::vector<float> m_cpuHistory;
    std::vector<float> m_gpuHistory;
    std::vector<float> m_fpsHistory;
};