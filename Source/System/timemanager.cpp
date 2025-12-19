#include "pch.h"

void TimeManager::initialize()
{
    m_profiler = CpuGpuProfiler();
}

void TimeManager::update()
{
    using namespace std::chrono;

    //! 現在時間
    auto now = high_resolution_clock::now();

    if (!m_initialized)
    {
        m_lastTime = now;
        m_startTime = now;
        m_initialized = true;
        return;
    }

    //! 経過時間計測（スケールなし）
    duration<float> diff = now - m_lastTime;
    m_unscaledDeltaTime = diff.count();
    m_lastTime = now;

    //! timeScale を適用した deltaTime
    m_deltaTime = m_unscaledDeltaTime * m_timeScale;

    //! 経過時間
    duration<float> fromStart = now - m_startTime;
    m_unscaledTime = fromStart.count();
    m_time = m_unscaledTime * m_timeScale;

    //! smoothDeltaTime の更新（簡易移動平均）
    const float smoothing = 0.1f;
    m_smoothDeltaTime = m_smoothDeltaTime * (1.0f - smoothing) + m_deltaTime * smoothing;
}

void TimeManager::frameStart(ID3D12GraphicsCommandList* cmd)
{
    //! プロファイラ更新(書き込み開始)
    m_profiler.beginFrame(cmd);
}

void TimeManager::frameEnd(ID3D12GraphicsCommandList* cmd)
{
    //! プロファイラ更新(書き込み終了)
    m_profiler.endFrame(cmd);

    //! FPS 計算
    calculateFPS();

    //! プロファイラにFPS記録
    m_profiler.recordFps(static_cast<float>(m_fps));
}

void TimeManager::imgui()
{
    if (ImGui::Begin("Performance Monitor"))   // ← 1つのウィンドウに統合
    {
        //! TimeManager 情報
        ImGui::Text("Delta: %.5f  (Unscaled: %.5f)", m_deltaTime, m_unscaledDeltaTime);
        ImGui::Text("Time : %.2f  (Unscaled: %.2f)", m_time, m_unscaledTime);
        ImGui::Text("SmoothDelta: %.5f", m_smoothDeltaTime);

        ImGui::SliderFloat("TimeScale", &m_timeScale, 0.0f, 3.0f);
        ImGui::Checkbox("Pause", &m_pause);
        if (m_pause)
            m_timeScale = 0.0f;

        ImGui::Separator();
        ImGui::Text("FPS: %d", m_fps);

        //! Profiler 情報
        CpuGpuProfiler& profiler = m_profiler;

        ImGui::Separator();
        ImGui::Text("CPU: %.3f ms  |  GPU: %.3f ms",
            profiler.getCpuTimems(),
            profiler.getGpuTimems()
        );

        //! 小型グラフ
        ImGui::PlotLines("CPU (ms)",
            profiler.cpuHistory().data(), static_cast<int>(profiler.cpuHistory().size()),
            0, nullptr, 0.0f, 30.0f, ImVec2(0, 60)
        );

        ImGui::PlotLines("GPU (ms)",
            profiler.gpuHistory().data(), static_cast<int>(profiler.gpuHistory().size()),
            0, nullptr, 0.0f, 30.0f, ImVec2(0, 60)
        );

        ImGui::PlotLines("FPS",
            profiler.fpsHistory().data(), static_cast<int>(profiler.fpsHistory().size()),
            0, nullptr, 0.0f, 120.0f, ImVec2(0, 60)
        );
    }
    ImGui::End();
}

void TimeManager::calculateFPS()
{
    m_fpsTimer += m_unscaledDeltaTime;
    m_fpsFrameCounter++;

    if (m_fpsTimer >= 1.0f)
    {
        m_fps = m_fpsFrameCounter;  //!< 1 秒間のフレーム数
        m_fpsFrameCounter = 0;
        m_fpsTimer = 0.0f;
    }
}