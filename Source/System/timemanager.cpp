#include "pch.h"
#include "Render/RenderPipeline.h"

void TimeManager::initialize()
{
    m_initialized = false;
    m_pause = false;
    m_deltaTime = 0.0f;
    m_unscaledDeltaTime = 0.0f;
    m_time = 0.0f;
    m_unscaledTime = 0.0f;
    m_smoothDeltaTime = 0.0f;
    m_timeScale = 1.0f;
    m_stepRequested = false;
    m_fps = 0;
    m_fpsTimer = 0.0f;
    m_fpsFrameCounter = 0;
}

void TimeManager::update()
{
    using namespace std::chrono;

    // 現在時間
    auto now = high_resolution_clock::now();

    if (!m_initialized)
    {
        m_lastTime = now;
        m_startTime = now;
        m_initialized = true;
        return;
    }

    // 経過時間計測（スケールなし）
    duration<float> diff = now - m_lastTime;
    m_unscaledDeltaTime = diff.count();
    m_lastTime = now;

    // timeScale を適用した deltaTime
    m_deltaTime = m_unscaledDeltaTime * m_timeScale;

    // 経過時間
    duration<float> fromStart = now - m_startTime;
    m_unscaledTime = fromStart.count();

    const float scale = std::max(m_timeScale, 0.0f);
    if (m_pause)
    {
        if (m_stepRequested)
        {
            const float stepScale = scale > 0.0f ? scale : 1.0f;
            m_deltaTime = m_stepDuration * stepScale;
            m_stepRequested = false;
        }
        else
        {
            m_deltaTime = 0.0f;
        }
    }
    else
    {
        m_deltaTime = m_unscaledDeltaTime * scale;
    }

    m_time += m_deltaTime;

    // smoothDeltaTime の更新（簡易移動平均）
    const float smoothing = 0.1f;
    m_smoothDeltaTime = m_smoothDeltaTime * (1.0f - smoothing) + m_deltaTime * smoothing;
}

void TimeManager::frameStart(ID3D12GraphicsCommandList* cmd)
{
    MemorySystem::Instance().beginFrame();

    // プロファイラ更新(書き込み開始)
    m_profiler.beginFrame(cmd);
}

void TimeManager::frameEnd(ID3D12GraphicsCommandList* cmd)
{
    // プロファイラ更新(書き込み終了)
    m_profiler.endFrame(cmd);

    // FPS 計算
    calculateFPS();

    // プロファイラにFPS記録
    m_profiler.recordFps(static_cast<float>(m_fps));
}

void TimeManager::imgui()
{
    if (ImGui::Begin("Profiler"))
    {
        renderProfilerContents();
    }
    ImGui::End();
}

void TimeManager::renderProfilerContents()
{
    ImGui::Text("Delta: %.5f  (Unscaled: %.5f)", m_deltaTime, m_unscaledDeltaTime);
    ImGui::Text("Time : %.2f  (Unscaled: %.2f)", m_time, m_unscaledTime);
    ImGui::Text("SmoothDelta: %.5f", m_smoothDeltaTime);

    float timeScale = m_timeScale;
    if (ImGui::SliderFloat("TimeScale", &timeScale, 0.0f, 3.0f))
    {
        setTimeScale(timeScale);
    }

    bool paused = m_pause;
    if (ImGui::Checkbox("Pause", &paused))
    {
        setPaused(paused);
    }

    ImGui::Separator();
    ImGui::Text("FPS: %d", m_fps);

    CpuGpuProfiler& profiler = m_profiler;

    ImGui::Separator();
    ImGui::Text("CPU: %.3f ms  |  GPU: %.3f ms",
        profiler.getCpuTimems(),
        profiler.getGpuTimems());
    ImGui::Text("CPU Memory: %.1f MB  |  GPU Memory: %.1f MB",
        profiler.getCpuMemoryMB(),
        profiler.getGpuMemoryMB());

    const MemorySystem& memory = MemorySystem::Instance();
    const MemorySystem::LeakStats leakStats = memory.getLeakStats();
    const MemorySystem::LinearAllocatorStats linearStats = memory.getLinearAllocatorStats();
    const MemorySystem::StackAllocatorStats stackStats = memory.getStackAllocatorStats();
    const MemorySystem::PoolAllocatorStats poolStats = memory.getPoolAllocatorStats();

    const float trackedActiveMB = static_cast<float>(leakStats.activeBytes) / (1024.0f * 1024.0f);
    const float trackedPeakMB = static_cast<float>(leakStats.peakActiveBytes) / (1024.0f * 1024.0f);

    ImGui::Text("Tracked Allocations: %llu active | %.2f MB active | %.2f MB peak",
        static_cast<unsigned long long>(leakStats.activeAllocations),
        trackedActiveMB,
        trackedPeakMB);

    const float gpuBudgetMB = memory.getGpuLocalBudgetMB();
    const float gpuUsageMB = memory.getGpuLocalUsageMB();
    const float gpuPeakMB = memory.getGpuLocalPeakMB();
    const float gpuUsageRatio = memory.getGpuLocalUsageRatio();

    ImGui::Text("GPU Local: %.1f / %.1f MB (peak %.1f MB)", gpuUsageMB, gpuBudgetMB, gpuPeakMB);
    ImGui::ProgressBar(std::clamp(gpuUsageRatio, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "GPU Budget Usage");

    const float linearUsage = linearStats.capacityBytes > 0
        ? static_cast<float>(linearStats.usedBytes) / static_cast<float>(linearStats.capacityBytes)
        : 0.0f;
    const float stackUsage = stackStats.capacityBytes > 0
        ? static_cast<float>(stackStats.usedBytes) / static_cast<float>(stackStats.capacityBytes)
        : 0.0f;
    const float poolUsage = poolStats.capacityBlocks > 0
        ? static_cast<float>(poolStats.activeBlocks) / static_cast<float>(poolStats.capacityBlocks)
        : 0.0f;

    ImGui::Text("Linear Allocator: %zu / %zu KB (peak %zu KB)",
        linearStats.usedBytes / 1024,
        linearStats.capacityBytes / 1024,
        linearStats.peakBytes / 1024);
    ImGui::ProgressBar(std::clamp(linearUsage, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Linear Usage");

    ImGui::Text("Stack Allocator: %zu / %zu KB (peak %zu KB)",
        stackStats.usedBytes / 1024,
        stackStats.capacityBytes / 1024,
        stackStats.peakBytes / 1024);
    ImGui::ProgressBar(std::clamp(stackUsage, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Stack Usage");

    ImGui::Text("Pool Allocator: %zu / %zu blocks (%zu B block, peak %zu blocks)",
        poolStats.activeBlocks,
        poolStats.capacityBlocks,
        poolStats.blockSizeBytes,
        poolStats.peakActiveBlocks);
    ImGui::ProgressBar(std::clamp(poolUsage, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), "Pool Usage");

    ImGui::PlotLines("CPU (ms)",
        profiler.cpuHistory().data(), static_cast<int>(profiler.cpuHistory().size()),
        0, nullptr, 0.0f, 30.0f, ImVec2(0, 60));

    ImGui::PlotLines("GPU (ms)",
        profiler.gpuHistory().data(), static_cast<int>(profiler.gpuHistory().size()),
        0, nullptr, 0.0f, 30.0f, ImVec2(0, 60));

    ImGui::PlotLines("FPS",
        profiler.fpsHistory().data(), static_cast<int>(profiler.fpsHistory().size()),
        0, nullptr, 0.0f, 120.0f, ImVec2(0, 60));

    ImGui::PlotLines("CPU Memory (MB)",
        profiler.cpuMemoryHistory().data(), static_cast<int>(profiler.cpuMemoryHistory().size()),
        0, nullptr, 0.0f, 4096.0f, ImVec2(0, 60));

    ImGui::PlotLines("GPU Memory (MB)",
        profiler.gpuMemoryHistory().data(), static_cast<int>(profiler.gpuMemoryHistory().size()),
        0, nullptr, 0.0f, 8192.0f, ImVec2(0, 60));

    ImGui::Separator();
    ImGui::Text("GPU Scope Timing (Timestamp Query)");

    const auto& scopes = profiler.gpuScopeSamples();
    if (scopes.empty())
    {
        ImGui::TextDisabled("No GPU scopes captured yet.");
    }
    else
    {
        float maxMs = 0.0f;
        for (const auto& scope : scopes)
        {
            maxMs = std::max(maxMs, scope.gpuMs);
        }
        maxMs = std::max(maxMs, 0.001f);

        for (const auto& scope : scopes)
        {
            const float ratio = std::clamp(scope.gpuMs / maxMs, 0.0f, 1.0f);

            char valueLabel[64] = {};
            std::snprintf(valueLabel, sizeof(valueLabel), "%.3f ms", scope.gpuMs);

            ImGui::TextUnformatted(scope.name.c_str());
            ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), valueLabel);
        }
    }
}

void TimeManager::play()
{
    m_pause = false;
}

void TimeManager::pause()
{
    m_pause = true;
}

void TimeManager::setPaused(bool paused)
{
    m_pause = paused;
}

void TimeManager::togglePause()
{
    m_pause = !m_pause;
}

void TimeManager::requestSingleStep()
{
    m_pause = true;
    m_stepRequested = true;
}

void TimeManager::setTimeScale(float value)
{
    m_timeScale = std::max(value, 0.0f);
}

void TimeManager::calculateFPS()
{
    m_fpsTimer += m_unscaledDeltaTime;
    m_fpsFrameCounter++;

    if (m_fpsTimer >= 1.0f)
    {
        m_fps = m_fpsFrameCounter;  // 1 秒間のフレーム数
        m_fpsFrameCounter = 0;
        m_fpsTimer = 0.0f;
    }
}