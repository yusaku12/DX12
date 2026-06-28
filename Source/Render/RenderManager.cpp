#include "pch.h"
#include "Component\IRenderComponent.h"
#include "Component\TransformComponent.h"
#include "HiZPyramid.h"

namespace
{
    DirectX::BoundingFrustum buildWorldFrustum(const CameraComponent* camera)
    {
        DirectX::BoundingFrustum localFrustum;
        DirectX::BoundingFrustum::CreateFromMatrix(localFrustum, camera->getProjection());

        DirectX::BoundingFrustum worldFrustum;
        const Matrix invView = camera->getView().Invert();
        localFrustum.Transform(worldFrustum, invView);
        return worldFrustum;
    }

    bool tryGetComponentCenter(const IRenderComponent* comp, Vector3& outCenter)
    {
        if (!comp)
        {
            return false;
        }

        Vector3 center{};
        Vector3 extents{};
        if (comp->getWorldAABB(center, extents))
        {
            outCenter = center;
            return true;
        }

        GameObject* go = comp->gameObject();
        if (!go)
        {
            return false;
        }

        if (auto* tf = go->getComponent<TransformComponent>())
        {
            outCenter = tf->getPosition();
            return true;
        }

        return false;
    }

    bool hasRenderableDescendant(GameObject* node, const std::unordered_set<GameObject*>& renderableSet)
    {
        if (!node)
        {
            return false;
        }

        for (GameObject* child : node->getChildren())
        {
            if (!child) continue;
            if (renderableSet.contains(child))
            {
                return true;
            }
            if (hasRenderableDescendant(child, renderableSet))
            {
                return true;
            }
        }

        return false;
    }

    void markDescendantRenderables(GameObject* node,
        const std::unordered_map<GameObject*, IRenderComponent*>& byObject,
        std::unordered_set<IRenderComponent*>& outMarked)
    {
        if (!node)
        {
            return;
        }

        for (GameObject* child : node->getChildren())
        {
            if (!child) continue;

            auto it = byObject.find(child);
            if (it != byObject.end() && it->second)
            {
                outMarked.insert(it->second);
            }

            markDescendantRenderables(child, byObject, outMarked);
        }
    }
}

std::vector<IRenderComponent*> RenderManager::copyComponents()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_components;
}

std::vector<IRenderComponent*> RenderManager::collectVisibleComponents(const std::vector<IRenderComponent*>& comps)
{
    m_lastSubmittedCount = comps.size();

    if (!m_enableFrustumCulling)
    {
        m_lastVisibleCount = comps.size();
        m_lastFrustumCulledCount = 0;
        return comps;
    }

    CameraComponent* camera = CameraManager::Instance().getMainCamera();
    if (!camera)
    {
        m_lastVisibleCount = comps.size();
        m_lastFrustumCulledCount = 0;
        return comps;
    }

    const DirectX::BoundingFrustum worldFrustum = buildWorldFrustum(camera);
    const auto isFiniteVec3 = [](const Vector3& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };

    std::vector<IRenderComponent*> visible;
    visible.reserve(comps.size());

    size_t culled = 0;
    for (IRenderComponent* comp : comps)
    {
        if (!comp)
        {
            continue;
        }

        Vector3 center{};
        Vector3 extents{};
        if (!comp->getWorldAABB(center, extents))
        {
            // 境界が取得できないものは安全側で描画
            visible.push_back(comp);
            continue;
        }

        // 不正境界は誤カリングを避けるため描画側へフォールバック
        if (!isFiniteVec3(center) || !isFiniteVec3(extents) ||
            extents.x < 0.0f || extents.y < 0.0f || extents.z < 0.0f)
        {
            visible.push_back(comp);
            continue;
        }

        const DirectX::BoundingBox box(center, extents);
        if (worldFrustum.Contains(box) == DirectX::DISJOINT)
        {
            ++culled;
            continue;
        }

        visible.push_back(comp);
    }

    // カメラ/境界の一時的不整合で全落ちするのを防ぐフェイルセーフ
    if (visible.empty() && !comps.empty())
    {
        m_lastVisibleCount = comps.size();
        m_lastFrustumCulledCount = 0;
        return comps;
    }

    m_lastVisibleCount = visible.size();
    m_lastFrustumCulledCount = culled;
    return visible;
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
        dx12.transitionSceneToRenderTarget(cmd);
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
    auto comps = collectVisibleComponents(copyComponents());
    if (comps.empty()) return;

    comps = applyAutoHlod(comps);
    if (comps.empty()) return;

    applyAutoLod(comps);

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

    auto comps = collectVisibleComponents(copyComponents());
    if (comps.empty()) return;

    comps = applyAutoHlod(comps);
    if (comps.empty()) return;

    applyAutoLod(comps);

    std::vector<IRenderComponent*> activeComps;
    activeComps.reserve(comps.size());
    for (auto* comp : comps)
    {
        activeComps.push_back(comp);
    }

    if (activeComps.empty()) return;

    if (activeComps.size() == 1)
    {
        auto start = Clock::now();

        if (kind == RenderPassKind::Default)
        {
            activeComps[0]->render();
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
                executeRender(kind, activeComps[0], cmd);
            }
        }

        auto end = Clock::now();
        recordSingleThreadTiming(activeComps[0]->getName(), std::chrono::duration<float, std::milli>(end - start).count());
        return;
    }

    auto& pool = CommandListPool::Instance();

    auto frameStart = Clock::now();
    clearTimings();

    std::vector<std::pair<ID3D12GraphicsCommandList*, std::future<void>>> tasks;
    tasks.reserve(activeComps.size());

    for (auto* comp : activeComps)
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

std::vector<IRenderComponent*> RenderManager::applyAutoHlod(const std::vector<IRenderComponent*>& comps)
{
    m_lastHlodMergedCount = 0;

    if (!m_enableAutoHlod)
    {
        return comps;
    }

    CameraComponent* camera = CameraManager::Instance().getMainCamera();
    if (!camera)
    {
        return comps;
    }

    std::unordered_map<GameObject*, IRenderComponent*> byObject;
    byObject.reserve(comps.size());
    std::unordered_set<GameObject*> renderableSet;
    renderableSet.reserve(comps.size());

    for (IRenderComponent* comp : comps)
    {
        if (!comp || !comp->gameObject())
        {
            continue;
        }
        byObject[comp->gameObject()] = comp;
        renderableSet.insert(comp->gameObject());
    }

    if (byObject.empty())
    {
        return comps;
    }

    std::unordered_set<IRenderComponent*> skip;
    const Vector3 cameraPos = camera->getPosition();

    for (IRenderComponent* comp : comps)
    {
        if (!comp || !comp->gameObject())
        {
            continue;
        }

        GameObject* go = comp->gameObject();
        if (!hasRenderableDescendant(go, renderableSet))
        {
            continue;
        }

        Vector3 center{};
        if (!tryGetComponentCenter(comp, center))
        {
            continue;
        }

        const float distance = Vector3::Distance(cameraPos, center);
        if (distance >= m_hlodSwitchDistance)
        {
            markDescendantRenderables(go, byObject, skip);
        }
        else
        {
            skip.insert(comp);
        }
    }

    if (skip.empty())
    {
        return comps;
    }

    std::vector<IRenderComponent*> filtered;
    filtered.reserve(comps.size());
    for (IRenderComponent* comp : comps)
    {
        if (!comp) continue;
        if (skip.contains(comp))
        {
            ++m_lastHlodMergedCount;
            continue;
        }
        filtered.push_back(comp);
    }

    if (filtered.empty())
    {
        m_lastHlodMergedCount = 0;
        return comps;
    }

    return filtered;
}

void RenderManager::applyAutoLod(const std::vector<IRenderComponent*>& comps)
{
    m_lastLodAdjustedCount = 0;

    if (!m_enableAutoLod)
    {
        for (IRenderComponent* comp : comps)
        {
            if (!comp) continue;
            comp->setRuntimeLodLevel(0);
        }
        return;
    }

    CameraComponent* camera = CameraManager::Instance().getMainCamera();
    if (!camera)
    {
        return;
    }

    const Vector3 cameraPos = camera->getPosition();
    for (IRenderComponent* comp : comps)
    {
        if (!comp) continue;
        const int lodLevel = comp->evaluateAutoLodLevel(cameraPos);
        comp->setRuntimeLodLevel(lodLevel);
        if (lodLevel > 0)
        {
            ++m_lastLodAdjustedCount;
        }
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

void RenderManager::renderShadowCasters(const DirectX::BoundingOrientedBox& cascadeOBB)
{
    auto comps = copyComponents();
    if (comps.empty()) return;

    auto cmd = DX12::Instance().getGraphicsCommandList();

    for (auto* comp : comps)
    {
        Vector3 center, extents;
        if (comp->getWorldAABB(center, extents))
        {
            DirectX::BoundingBox box(center, extents);
            if (cascadeOBB.Contains(box) == DirectX::DISJOINT)
            {
                continue; //!< カスケード範囲外なので影描画から除外（culling）
            }
        }
        comp->renderShadowDepth(cmd);
    }
}

void RenderManager::debugImgui()
{
    if (!ImGui::Begin("Rendering"))
    {
        ImGui::End();
        return;
    }

    renderDebugContents();

    ImGui::End();
}

void RenderManager::renderDebugContents()
{

    size_t componentCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        componentCount = m_components.size();
    }

    ImGui::Text("Components: %zu", componentCount);
    ImGui::Checkbox("Use Multi-Threaded", &m_useMultiThreaded);
    ImGui::Checkbox("Frustum Culling", &m_enableFrustumCulling);
    ImGui::Checkbox("Auto LOD", &m_enableAutoLod);
    ImGui::Checkbox("Auto HLOD", &m_enableAutoHlod);
    ImGui::DragFloat("HLOD Switch Distance", &m_hlodSwitchDistance, 0.5f, 0.0f, 5000.0f);

    ImGui::Text("Submitted: %zu", m_lastSubmittedCount);
    ImGui::Text("Visible: %zu", m_lastVisibleCount);
    ImGui::Text("Frustum Culled: %zu", m_lastFrustumCulledCount);
    ImGui::Text("HLOD Skipped: %zu", m_lastHlodMergedCount);
    ImGui::Text("LOD Adjusted: %zu", m_lastLodAdjustedCount);

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
}