#include "pch.h"
#include "Render/RenderManager.h"
#include "System/TimeManager.h"
#include "Camera/CameraManager.h"
#include "Render/GBufferRenderTargets.h"
#include "Camera/CameraComponent.h"
#include "Component\IRenderComponent.h"
#include "Component\TransformComponent.h"
#include "Graphics/GpuDebugMarker.h"
#include "HiZPyramid.h"
#include "DynamicResolutionManager.h"

namespace
{
    constexpr UINT kMinOcclusionQueryCapacity = 1024;
}

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

void RenderManager::ensureOcclusionCapacity(UINT requiredCount)
{
    if (requiredCount == 0)
    {
        requiredCount = kMinOcclusionQueryCapacity;
    }

    requiredCount = std::max(requiredCount, kMinOcclusionQueryCapacity);

    if (m_occlusionState.capacity >= requiredCount && m_occlusionState.queryHeap && m_occlusionState.readbackBuffer)
    {
        return;
    }

    ID3D12Device* device = DX12::Instance().getDevice();
    if (!device)
    {
        return;
    }

    D3D12_QUERY_HEAP_DESC heapDesc{};
    heapDesc.Count = requiredCount;
    heapDesc.NodeMask = 0;
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap;
    HRESULT hr = device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(queryHeap.GetAddressOf()));
    if (FAILED(hr))
    {
        LOG_HR(hr, "[RenderManager] Failed to create occlusion query heap");
        return;
    }

    D3D12_RESOURCE_DESC readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(requiredCount) * sizeof(UINT64));
    CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);

    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    hr = device->CreateCommittedResource(
        &readbackHeap,
        D3D12_HEAP_FLAG_NONE,
        &readbackDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(readback.GetAddressOf()));
    if (FAILED(hr))
    {
        LOG_HR(hr, "[RenderManager] Failed to create occlusion readback buffer");
        return;
    }

    m_occlusionState.queryHeap = std::move(queryHeap);
    m_occlusionState.readbackBuffer = std::move(readback);
    m_occlusionState.capacity = requiredCount;
    m_occlusionState.used = 0;
    m_occlusionState.indexToComponent.assign(requiredCount, nullptr);
}

void RenderManager::beginOcclusionFrame()
{
    if (!m_enableGpuOcclusion)
    {
        m_occlusionFramePrepared = true;
        m_lastOcclusionQueryCount = 0;
        return;
    }

    std::lock_guard<std::mutex> lock(m_occlusionMutex);

    ensureOcclusionCapacity(static_cast<UINT>(std::max<size_t>(m_components.size() * 3, kMinOcclusionQueryCapacity)));
    if (!m_occlusionState.readbackBuffer)
    {
        m_occlusionFramePrepared = true;
        return;
    }

    if (m_occlusionState.used > 0)
    {
        void* mapped = nullptr;
        D3D12_RANGE readRange{};
        readRange.Begin = 0;
        readRange.End = static_cast<SIZE_T>(m_occlusionState.used) * sizeof(UINT64);
        HRESULT hr = m_occlusionState.readbackBuffer->Map(0, &readRange, &mapped);
        if (SUCCEEDED(hr) && mapped)
        {
            const UINT64* samples = static_cast<const UINT64*>(mapped);
            for (UINT i = 0; i < m_occlusionState.used; ++i)
            {
                IRenderComponent* comp = m_occlusionState.indexToComponent[i];
                if (!comp)
                {
                    continue;
                }

                m_occlusionVisibleByComponent[comp] = (samples[i] > 0);
            }

            D3D12_RANGE writeRange{};
            writeRange.Begin = 0;
            writeRange.End = 0;
            m_occlusionState.readbackBuffer->Unmap(0, &writeRange);
        }
        else
        {
            LOG_HR(hr, "[RenderManager] Failed to map occlusion readback buffer");
        }
    }

    m_lastOcclusionQueryCount = m_occlusionState.used;
    m_occlusionState.used = 0;
    std::fill(m_occlusionState.indexToComponent.begin(), m_occlusionState.indexToComponent.end(), nullptr);
    m_occlusionFramePrepared = true;
}

bool RenderManager::allocateOcclusionQueryIndex(IRenderComponent* comp, UINT& outIndex)
{
    outIndex = UINT_MAX;
    if (!comp)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_occlusionMutex);
    if (!m_enableGpuOcclusion || !m_occlusionState.queryHeap || !m_occlusionState.readbackBuffer)
    {
        return false;
    }

    if (m_occlusionState.used >= m_occlusionState.capacity)
    {
        return false;
    }

    outIndex = m_occlusionState.used++;
    m_occlusionState.indexToComponent[outIndex] = comp;
    return true;
}

bool RenderManager::shouldIssueOcclusionQuery(IRenderComponent* comp) const
{
    if (!m_enableGpuOcclusion || !comp)
    {
        return false;
    }

    Vector3 center{};
    Vector3 extents{};
    if (!comp->getWorldAABB(center, extents))
    {
        return false;
    }

    constexpr float kMinExtent = 0.005f;
    return extents.x > kMinExtent || extents.y > kMinExtent || extents.z > kMinExtent;
}

void RenderManager::executeWithOcclusionQuery(ID3D12GraphicsCommandList* cmd, IRenderComponent* comp, const std::function<void()>& drawFn)
{
    if (!drawFn)
    {
        return;
    }

    if (!cmd || !shouldIssueOcclusionQuery(comp))
    {
        drawFn();
        return;
    }

    UINT queryIndex = UINT_MAX;
    if (!allocateOcclusionQueryIndex(comp, queryIndex))
    {
        drawFn();
        return;
    }

    cmd->BeginQuery(m_occlusionState.queryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIndex);
    drawFn();
    cmd->EndQuery(m_occlusionState.queryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIndex);
    cmd->ResolveQueryData(
        m_occlusionState.queryHeap.Get(),
        D3D12_QUERY_TYPE_BINARY_OCCLUSION,
        queryIndex,
        1,
        m_occlusionState.readbackBuffer.Get(),
        static_cast<UINT64>(queryIndex) * sizeof(UINT64));
}

bool RenderManager::isOcclusionVisible(IRenderComponent* comp) const
{
    if (!m_enableGpuOcclusion || !comp)
    {
        return true;
    }

    auto it = m_occlusionVisibleByComponent.find(comp);
    if (it == m_occlusionVisibleByComponent.end())
    {
        return true;
    }

    return it->second;
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

    size_t occlusionCulled = 0;
    if (m_enableGpuOcclusion)
    {
        std::vector<IRenderComponent*> occlusionVisible;
        occlusionVisible.reserve(visible.size());

        for (IRenderComponent* comp : visible)
        {
            if (!comp)
            {
                continue;
            }

            if (!shouldIssueOcclusionQuery(comp) || isOcclusionVisible(comp))
            {
                occlusionVisible.push_back(comp);
                continue;
            }

            ++occlusionCulled;
        }

        if (!occlusionVisible.empty())
        {
            visible = std::move(occlusionVisible);
        }
    }

    // カメラ/境界の一時的不整合で全落ちするのを防ぐフェイルセーフ
    if (visible.empty() && !comps.empty())
    {
        m_lastVisibleCount = comps.size();
        m_lastFrustumCulledCount = 0;
        m_lastOcclusionCulledCount = 0;
        return comps;
    }

    m_lastVisibleCount = visible.size();
    m_lastFrustumCulledCount = culled;
    m_lastOcclusionCulledCount = occlusionCulled;
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

    {
        std::lock_guard<std::mutex> lock(m_occlusionMutex);
        m_occlusionState = {};
        m_occlusionVisibleByComponent.clear();
        m_occlusionFramePrepared = false;
        m_lastOcclusionCulledCount = 0;
        m_lastOcclusionQueryCount = 0;
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
    std::string scopeName;
    if (cmd && comp)
    {
        const char* passLabel = "Default";
        if (kind == RenderPassKind::GBuffer)
        {
            passLabel = "GBuffer";
        }
        else if (kind == RenderPassKind::Forward)
        {
            passLabel = "Forward";
        }

        scopeName = std::format("{}::{}", passLabel, comp->getName());
    }

    GpuDebugMarker::ScopedEvent marker(cmd, scopeName.c_str());
    auto gpuToken = TimeManager::Instance().beginGpuScope(cmd, scopeName.c_str());

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

    TimeManager::Instance().endGpuScope(cmd, gpuToken);
}

void RenderManager::renderSingleThreadedInternal(RenderPassKind kind)
{
    if (!m_occlusionFramePrepared)
    {
        beginOcclusionFrame();
    }

    auto comps = collectVisibleComponents(copyComponents());
    if (comps.empty()) return;

    comps = applyAutoHlod(comps);
    if (comps.empty()) return;

    applyAutoLod(comps);

    auto* cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    if (kind == RenderPassKind::Default)
    {
        for (auto* comp : comps)
        {
            executeWithOcclusionQuery(cmd, comp, [this, kind, comp, cmd]()
                {
                    executeRender(kind, comp, cmd);
                });
        }
        return;
    }

    for (auto* comp : comps)
    {
        executeWithOcclusionQuery(cmd, comp, [this, kind, comp, cmd]()
            {
                executeRender(kind, comp, cmd);
            });
    }
}

void RenderManager::renderMultiThreadedInternal(RenderPassKind kind)
{
    using Clock = std::chrono::high_resolution_clock;

    if (!m_occlusionFramePrepared)
    {
        beginOcclusionFrame();
    }

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
        auto* cmd = DX12::Instance().getGraphicsCommandList();
        if (!cmd) return;

        if (kind == RenderPassKind::Forward)
        {
            setupCommandList(kind, cmd);
        }

        executeWithOcclusionQuery(cmd, activeComps[0], [this, kind, cmd, &activeComps]()
            {
                executeRender(kind, activeComps[0], cmd);
            });

        auto end = Clock::now();
        recordSingleThreadTiming(activeComps[0]->getName(), std::chrono::duration<float, std::milli>(end - start).count());
        return;
    }

    auto& pool = CommandListPool::Instance();

    auto frameStart = Clock::now();
    clearTimings();

    std::vector<ID3D12GraphicsCommandList*> commandLists;
    commandLists.reserve(activeComps.size());

    tf::Taskflow taskflow;
    const auto frameStartCopy = frameStart;

    for (auto* comp : activeComps)
    {
        auto* cmd = pool.acquire();
        setupCommandList(kind, cmd);
        commandLists.push_back(cmd);

        taskflow.emplace(
            [this, comp, cmd, frameStartCopy, kind]()
            {
                using Clock = std::chrono::high_resolution_clock;
                auto start = Clock::now();

                executeWithOcclusionQuery(cmd, comp, [this, kind, comp, cmd]()
                    {
                        executeRender(kind, comp, cmd);
                    });

                auto end = Clock::now();
                float startMs = std::chrono::duration<float, std::milli>(start - frameStartCopy).count();
                float durationMs = std::chrono::duration<float, std::milli>(end - start).count();

                addTiming(comp->getName(), startMs, durationMs, std::this_thread::get_id());
            });
    }

    m_taskExecutor.run(taskflow).wait();

    float totalMs = std::chrono::duration<float, std::milli>(Clock::now() - frameStart).count();
    finalizeTimings(totalMs);

    for (auto* cmd : commandLists)
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
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find(m_components.begin(), m_components.end(), comp);
        if (it != m_components.end())
        {
            m_components.erase(it);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_occlusionMutex);
        m_occlusionVisibleByComponent.erase(comp);
        for (IRenderComponent*& mapped : m_occlusionState.indexToComponent)
        {
            if (mapped == comp)
            {
                mapped = nullptr;
            }
        }
    }

    if (comp)
    {
        m_lastOcclusionCulledCount = 0;
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
    if (!m_occlusionFramePrepared)
    {
        beginOcclusionFrame();
    }

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
    ImGui::Checkbox("GPU Occlusion Query", &m_enableGpuOcclusion);
    ImGui::DragFloat("HLOD Switch Distance", &m_hlodSwitchDistance, 0.5f, 0.0f, 5000.0f);

    ImGui::Text("Submitted: %zu", m_lastSubmittedCount);
    ImGui::Text("Visible: %zu", m_lastVisibleCount);
    ImGui::Text("Frustum Culled: %zu", m_lastFrustumCulledCount);
    ImGui::Text("Occlusion Culled: %zu", m_lastOcclusionCulledCount);
    ImGui::Text("Occlusion Queries: %zu", m_lastOcclusionQueryCount);
    ImGui::Text("HLOD Skipped: %zu", m_lastHlodMergedCount);
    ImGui::Text("LOD Adjusted: %zu", m_lastLodAdjustedCount);

    DynamicResolutionManager::Instance().renderDebugContents();

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