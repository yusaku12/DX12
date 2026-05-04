#include "pch.h"
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
    // 安全のためローカルコピーを使う
    std::vector<IRenderComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    for (auto* comp : comps)
    {
        comp->render();
    }
}

void RenderManager::renderGBuffer()
{
    std::vector<IRenderComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    auto cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    for (auto* comp : comps)
    {
        comp->renderGBuffer(cmd);
    }
}

void RenderManager::renderForward()
{
    std::vector<IRenderComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    auto cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    for (auto* comp : comps)
    {
        comp->renderForward(cmd);
    }
}

void RenderManager::renderMultiThreaded()
{
    using Clock = std::chrono::high_resolution_clock;

    // ローカルコピー
    std::vector<IRenderComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    if (comps.empty()) return;

    // コンポーネントが1つならシングルスレッドで描画
    if (comps.size() == 1)
    {
        auto start = Clock::now();
        comps[0]->render();
        auto end = Clock::now();

        {
            std::lock_guard<std::mutex> lock(m_timingMutex);
            m_timings.clear();
            m_totalMs = std::chrono::duration<float, std::milli>(end - start).count();
            m_singleEstimateMs = m_totalMs;
            // 名前をコピーして保持する
            m_timings.push_back({ comps[0]->getName(), 0.0f, m_totalMs, std::this_thread::get_id() });
        }
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
    tasks.reserve(comps.size());

    for (auto* comp : comps)
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
                // 名前はコピーして保存
                m_timings.push_back({ comp->getName(), startMs, durationMs, std::this_thread::get_id() });
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
    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        for (const auto& t : m_timings)
            m_singleEstimateMs += t.durationMs;
    }

    // コマンドリスト返却
    for (auto& [cmd, future] : tasks)
    {
        pool.release(cmd);
    }
}

void RenderManager::renderMultiThreadedGBuffer()
{
    using Clock = std::chrono::high_resolution_clock;

    std::vector<IRenderComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    if (comps.empty()) return;

    // コンポーネントが1つならシングルスレッドで描画
    if (comps.size() == 1)
    {
        auto start = Clock::now();
        auto cmd = DX12::Instance().getGraphicsCommandList();
        if (cmd)
        {
            comps[0]->renderGBuffer(cmd);
        }
        auto end = Clock::now();

        {
            std::lock_guard<std::mutex> lock(m_timingMutex);
            m_timings.clear();
            m_totalMs = std::chrono::duration<float, std::milli>(end - start).count();
            m_singleEstimateMs = m_totalMs;
            m_timings.push_back({ comps[0]->getName(), 0.0f, m_totalMs, std::this_thread::get_id() });
        }
        return;
    }

    auto& dx12 = DX12::Instance();
    auto& pool = CommandListPool::Instance();
    auto& gbuffer = GBufferRenderTargets::Instance();

    auto frameStart = Clock::now();

    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_timings.clear();
    }

    std::vector<std::pair<ID3D12GraphicsCommandList*, std::future<void>>> tasks;
    tasks.reserve(comps.size());

    for (auto* comp : comps)
    {
        auto* cmd = pool.acquire();
        dx12.applyViewportAndScissor(cmd);
        gbuffer.setRenderTargets(cmd, dx12.getDSVHandle());

        auto future = std::async(std::launch::async,
            [this, comp, cmd, &frameStart]()
            {
                using Clock = std::chrono::high_resolution_clock;
                auto start = Clock::now();

                comp->renderGBuffer(cmd);

                auto end = Clock::now();
                float startMs = std::chrono::duration<float, std::milli>(start - frameStart).count();
                float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

                std::lock_guard<std::mutex> lock(m_timingMutex);
                m_timings.push_back({ comp->getName(), startMs, durationMs, std::this_thread::get_id() });
            });

        tasks.push_back({ cmd, std::move(future) });
    }

    for (auto& [cmd, future] : tasks)
    {
        future.wait();
    }

    m_totalMs = std::chrono::duration<float, std::milli>(Clock::now() - frameStart).count();

    m_singleEstimateMs = 0.0f;
    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        for (const auto& t : m_timings)
            m_singleEstimateMs += t.durationMs;
    }

    for (auto& [cmd, future] : tasks)
    {
        pool.release(cmd);
    }
}

void RenderManager::renderMultiThreadedForward()
{
    using Clock = std::chrono::high_resolution_clock;

    std::vector<IRenderComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    if (comps.empty()) return;

    if (comps.size() == 1)
    {
        auto start = Clock::now();
        auto cmd = DX12::Instance().getGraphicsCommandList();
        if (cmd)
        {
            DX12::Instance().applyViewportAndScissor(cmd);
            DX12::Instance().applySceneRenderTargets(cmd);
            comps[0]->renderForward(cmd);
        }
        auto end = Clock::now();

        {
            std::lock_guard<std::mutex> lock(m_timingMutex);
            m_timings.clear();
            m_totalMs = std::chrono::duration<float, std::milli>(end - start).count();
            m_singleEstimateMs = m_totalMs;
            m_timings.push_back({ comps[0]->getName(), 0.0f, m_totalMs, std::this_thread::get_id() });
        }
        return;
    }

    auto& dx12 = DX12::Instance();
    auto& pool = CommandListPool::Instance();

    auto frameStart = Clock::now();

    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        m_timings.clear();
    }

    std::vector<std::pair<ID3D12GraphicsCommandList*, std::future<void>>> tasks;
    tasks.reserve(comps.size());

    for (auto* comp : comps)
    {
        auto* cmd = pool.acquire();
        dx12.applyViewportAndScissor(cmd);
        dx12.applySceneRenderTargets(cmd);

        auto future = std::async(std::launch::async,
            [this, comp, cmd, &frameStart]()
            {
                using Clock = std::chrono::high_resolution_clock;
                auto start = Clock::now();

                comp->renderForward(cmd);

                auto end = Clock::now();
                float startMs = std::chrono::duration<float, std::milli>(start - frameStart).count();
                float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

                std::lock_guard<std::mutex> lock(m_timingMutex);
                m_timings.push_back({ comp->getName(), startMs, durationMs, std::this_thread::get_id() });
            });

        tasks.push_back({ cmd, std::move(future) });
    }

    for (auto& [cmd, future] : tasks)
    {
        future.wait();
    }

    m_totalMs = std::chrono::duration<float, std::milli>(Clock::now() - frameStart).count();

    m_singleEstimateMs = 0.0f;
    {
        std::lock_guard<std::mutex> lock(m_timingMutex);
        for (const auto& t : m_timings)
            m_singleEstimateMs += t.durationMs;
    }

    for (auto& [cmd, future] : tasks)
    {
        pool.release(cmd);
    }
}