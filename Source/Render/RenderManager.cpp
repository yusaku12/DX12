#include "pch.h"
#include "RenderManager.h"
#include "Component\IRenderComponent.h"

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
    for (auto* comp : m_components)
    {
        comp->render();
    }
}

void RenderManager::renderMultiThreaded()
{
    using Clock = std::chrono::high_resolution_clock;

    if (m_components.empty()) return;

    // コンポーネントが1つならシングルスレッドで描画
    if (m_components.size() == 1)
    {
        auto start = Clock::now();
        m_components[0]->render();
        auto end = Clock::now();

        m_timings.clear();
        m_totalMs = std::chrono::duration<float, std::milli>(end - start).count();
        m_singleEstimateMs = m_totalMs;
        m_timings.push_back({ m_components[0]->getRenderName(), 0.0f, m_totalMs, std::this_thread::get_id() });
        return;
    }

    auto& dx12 = DX12::Instance();
    auto& pool = CommandListPool::Instance();

    auto frameStart = Clock::now();

    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_timings.clear();
    }

    // コンポーネント毎にコマンドリストを取得 & 非同期実行
    std::vector<std::pair<ID3D12GraphicsCommandList*, std::future<void>>> tasks;
    tasks.reserve(m_components.size());

    for (auto* comp : m_components)
    {
        auto* cmd = pool.acquire();
        dx12.applyViewportAndScissor(cmd);
        dx12.applySceneRenderTargets(cmd);

        auto future = std::async(std::launch::async,
            [this, comp, cmd, &frameStart]()
            {
                using Clock = std::chrono::high_resolution_clock;
                auto start = Clock::now();

                comp->render(cmd);

                auto end = Clock::now();
                float startMs = std::chrono::duration<float, std::milli>(start - frameStart).count();
                float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

                std::lock_guard<std::mutex> lock(m_timingMutex);
                m_timings.push_back({ comp->getRenderName(), startMs, durationMs, std::this_thread::get_id() });
            });

        tasks.push_back({ cmd, std::move(future) });
    }

    // 全タスク完了を待つ
    for (auto& [cmd, future] : tasks)
    {
        future.wait();
    }

    m_totalMs = std::chrono::duration<float, std::milli>(Clock::now() - frameStart).count();

    // シングルスレッド推定値
    m_singleEstimateMs = 0.0f;
    for (const auto& t : m_timings)
        m_singleEstimateMs += t.durationMs;

    // コマンドリスト返却
    for (auto& [cmd, future] : tasks)
    {
        pool.release(cmd);
    }
}

void RenderManager::debugImgui()
{
    if (!ImGui::Begin("MT Command Recording"))
    {
        ImGui::End();
        return;
    }

    float speedup = (m_totalMs > 0.001f)
        ? m_singleEstimateMs / m_totalMs
        : 0.0f;

    ImGui::Text("Multi-thread total : %.3f ms", m_totalMs);
    ImGui::Text("Single-thread est. : %.3f ms", m_singleEstimateMs);
    ImGui::Text("Speedup            : %.2fx", speedup);

    ImGui::Separator();
    ImGui::Text("Thread Timeline:");

    float maxTime = m_totalMs;
    if (maxTime < 0.001f) maxTime = 1.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    float canvasWidth = ImGui::GetContentRegionAvail().x;

    float barHeight = 24.0f;
    float padding = 4.0f;

    ImU32 colors[] =
    {
        IM_COL32(66,135,245,255),
        IM_COL32(245,166,35,255),
        IM_COL32(80,200,120,255),
        IM_COL32(220,80,80,255),
    };

    for (size_t i = 0; i < m_timings.size(); ++i)
    {
        const auto& t = m_timings[i];

        float x0 = canvasPos.x + (t.startMs / maxTime) * canvasWidth;
        float x1 = canvasPos.x + ((t.startMs + t.durationMs) / maxTime) * canvasWidth;

        float y0 = canvasPos.y + i * (barHeight + padding);
        float y1 = y0 + barHeight;

        ImU32 color = colors[i % 4];

        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, 4.0f);

        char label[128];
        std::snprintf(label, sizeof(label),
            "%s  %.2f ms (TID:%zu)",
            t.name,
            t.durationMs,
            std::hash<std::thread::id>{}(t.threadId) % 10000);

        drawList->AddText(ImVec2(x0 + 4.0f, y0 + 4.0f),
            IM_COL32(255, 255, 255, 255),
            label);
    }

    float totalHeight =
        static_cast<float>(m_timings.size()) *
        (barHeight + padding) + padding;

    ImGui::Dummy(ImVec2(canvasWidth, totalHeight));

    ImGui::Separator();

    if (ImGui::BeginTable("ThreadDetails", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Task");
        ImGui::TableSetupColumn("Start (ms)");
        ImGui::TableSetupColumn("Duration (ms)");
        ImGui::TableSetupColumn("Thread ID");
        ImGui::TableHeadersRow();

        for (const auto& t : m_timings)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", t.name);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", t.startMs);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", t.durationMs);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu",
                std::hash<std::thread::id>{}(t.threadId) % 10000);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}