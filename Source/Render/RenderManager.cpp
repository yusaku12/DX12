#include "pch.h"
#include "Component\IRenderComponent.h"

std::vector<IRenderComponent*> RenderManager::copyComponents()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_components;
}

void RenderManager::recordSingleThreadTiming(const std::string& name, float totalMs)
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    m_timings.clear();
    m_totalMs = totalMs;
    m_singleEstimateMs = totalMs;
    m_timings.push_back({ name, 0.0f, totalMs, std::this_thread::get_id() });
}

void RenderManager::clearTimings()
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    m_timings.clear();
}

void RenderManager::addTiming(const std::string& name, float startMs, float durationMs, std::thread::id threadId)
{
    std::lock_guard<std::mutex> lock(m_timingMutex);
    m_timings.push_back({ name, startMs, durationMs, threadId });
}

void RenderManager::finalizeTimings(float totalMs)
{
    float singleEstimateMs = 0.0f;
    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        for (const auto& t : m_timings)
            singleEstimateMs += t.durationMs;

        m_totalMs = totalMs;
        m_singleEstimateMs = singleEstimateMs;
    }
}

void RenderManager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_components.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_timings.clear();
        m_totalMs = 0.0f;
        m_singleEstimateMs = 0.0f;
    }
}

void RenderManager::setupCommandList(RenderPassKind kind, ID3D12GraphicsCommandList* cmd)
{
    auto& dx12 = DX12::Instance();
    dx12.applyViewportAndScissor(cmd);

    if (kind == RenderPassKind::GBuffer)
    {
        GBufferRenderTargets::Instance().setRenderTargets(cmd, dx12.getDSVHandle());
    }
    else
    {
        dx12.applySceneRenderTargets(cmd);
    }
}

void RenderManager::executeRender(RenderPassKind kind, IRenderComponent* comp, ID3D12GraphicsCommandList* cmd)
{
    switch (kind)
    {
    case RenderPassKind::Default:
        if (cmd)
            comp->render(cmd);
        else
            comp->render();
        break;
    case RenderPassKind::GBuffer:
        comp->renderGBuffer(cmd);
        break;
    case RenderPassKind::Forward:
        comp->renderForward(cmd);
        break;
    }
}

void RenderManager::renderSingleThreadedInternal(RenderPassKind kind)
{
    auto comps = copyComponents();
    if (comps.empty()) return;

    if (kind == RenderPassKind::Default)
    {
        for (auto* comp : comps)
        {
            comp->render();
        }
        return;
    }

    auto* cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    for (auto* comp : comps)
    {
        executeRender(kind, comp, cmd);
    }
}

void RenderManager::renderMultiThreadedInternal(RenderPassKind kind)
{
    using Clock = std::chrono::high_resolution_clock;

    auto comps = copyComponents();
    if (comps.empty()) return;

    if (comps.size() == 1)
    {
        auto start = Clock::now();

        if (kind == RenderPassKind::Default)
        {
            comps[0]->render();
        }
        else
        {
            auto* cmd = DX12::Instance().getGraphicsCommandList();
            if (cmd)
            {
                if (kind == RenderPassKind::Forward)
                {
                    setupCommandList(kind, cmd);
                }
                executeRender(kind, comps[0], cmd);
            }
        }

        auto end = Clock::now();
        recordSingleThreadTiming(comps[0]->getName(), std::chrono::duration<float, std::milli>(end - start).count());
        return;
    }

    auto& pool = CommandListPool::Instance();

    auto frameStart = Clock::now();
    clearTimings();

    std::vector<std::pair<ID3D12GraphicsCommandList*, std::future<void>>> tasks;
    tasks.reserve(comps.size());

    for (auto* comp : comps)
    {
        auto* cmd = pool.acquire();
        setupCommandList(kind, cmd);

        auto future = std::async(std::launch::async,
            [this, comp, cmd, &frameStart, kind]()
            {
                using Clock = std::chrono::high_resolution_clock;
                auto start = Clock::now();

                executeRender(kind, comp, cmd);

                auto end = Clock::now();
                float startMs = std::chrono::duration<float, std::milli>(start - frameStart).count();
                float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

                addTiming(comp->getName(), startMs, durationMs, std::this_thread::get_id());
            });

        tasks.push_back({ cmd, std::move(future) });
    }

    for (auto& [cmd, future] : tasks)
    {
        future.wait();
    }

    float totalMs = std::chrono::duration<float, std::milli>(Clock::now() - frameStart).count();
    finalizeTimings(totalMs);

    for (auto& [cmd, future] : tasks)
    {
        pool.release(cmd);
    }
}

void RenderManager::registerComponent(IRenderComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 二重登録防止
    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it == m_components.end())
    {
        m_components.push_back(comp);
    }
}

void RenderManager::unregisterComponent(IRenderComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it != m_components.end())
    {
        m_components.erase(it);
    }
}

void RenderManager::render()
{
    renderSingleThreadedInternal(RenderPassKind::Default);
}

void RenderManager::renderGBuffer()
{
    renderSingleThreadedInternal(RenderPassKind::GBuffer);
}

void RenderManager::renderForward()
{
    renderSingleThreadedInternal(RenderPassKind::Forward);
}

void RenderManager::renderMultiThreaded()
{
    renderMultiThreadedInternal(RenderPassKind::Default);
}

void RenderManager::renderMultiThreadedGBuffer()
{
    renderMultiThreadedInternal(RenderPassKind::GBuffer);
}

void RenderManager::renderMultiThreadedForward()
{
    renderMultiThreadedInternal(RenderPassKind::Forward);
}

void RenderManager::debugImgui()
{
    if (!ImGui::Begin("RenderManager"))
    {
        ImGui::End();
        return;
    }

    size_t componentCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        componentCount = m_components.size();
    }

    ImGui::Text("Components: %zu", componentCount);
    ImGui::Checkbox("Use Multi-Threaded", &m_useMultiThreaded);

    std::vector<ThreadTimingInfo> timings;
    float totalMs = 0.0f;
    float singleEstimateMs = 0.0f;
    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        timings = m_timings;
        totalMs = m_totalMs;
        singleEstimateMs = m_singleEstimateMs;
    }

    ImGui::Separator();
    ImGui::Text("Total: %.3f ms", totalMs);
    ImGui::Text("Single Estimate: %.3f ms", singleEstimateMs);

    ImGui::Separator();
    if (timings.empty())
    {
        ImGui::TextDisabled(reinterpret_cast<const char*>(u8"計測データはありません"));
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("RenderManagerTimings", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Start (ms)");
        ImGui::TableSetupColumn("Duration (ms)");
        ImGui::TableSetupColumn("Thread");
        ImGui::TableHeadersRow();

        for (const auto& timing : timings)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", timing.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", timing.startMs);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", timing.durationMs);
            ImGui::TableSetColumnIndex(3);
            auto threadValue = std::hash<std::thread::id>{}(timing.threadId);
            ImGui::Text("0x%zx", threadValue);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}