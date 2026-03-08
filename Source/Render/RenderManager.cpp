#include "pch.h"
#include "RenderManager.h"
#include "Component\IRenderComponent.h"

//=====================================================
// 登録
//=====================================================
void RenderManager::registerComponent(IRenderComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    //! 二重登録防止
    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it == m_components.end())
    {
        m_components.push_back(comp);
    }
}

//=====================================================
// 登録解除
//=====================================================
void RenderManager::unregisterComponent(IRenderComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it != m_components.end())
    {
        m_components.erase(it);
    }
}

//=====================================================
// シングルスレッド描画
//=====================================================
void RenderManager::render()
{
    for (auto* comp : m_components)
    {
        comp->render();
    }
}

//=====================================================
// マルチスレッド描画
//=====================================================
void RenderManager::renderMultiThreaded()
{
    using Clock = std::chrono::high_resolution_clock;

    if (m_components.empty()) return;

    //! コンポーネントが1つならシングルスレッドで描画
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

    //! コンポーネント毎にコマンドリストを取得 & 非同期実行
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

    //! 全タスク完了を待つ
    for (auto& [cmd, future] : tasks)
    {
        future.wait();
    }

    m_totalMs = std::chrono::duration<float, std::milli>(Clock::now() - frameStart).count();

    //! シングルスレッド推定値
    m_singleEstimateMs = 0.0f;
    for (const auto& t : m_timings)
        m_singleEstimateMs += t.durationMs;

    //! コマンドリスト返却
    for (auto& [cmd, future] : tasks)
    {
        pool.release(cmd);
    }
}

//=====================================================
// デバック描画
//=====================================================
void RenderManager::debugRender()
{
}