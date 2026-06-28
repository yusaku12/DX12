#include "pch.h"
#include "CpuGpuProfiler.h"

#include <psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace
{
    constexpr size_t HistoryLength = 180;
    constexpr size_t MaxScopeSamples = 32;

    bool isValidPair(UINT start, UINT end)
    {
        return start != UINT_MAX && end != UINT_MAX && end >= start;
    }
}

CpuGpuProfiler::CpuGpuProfiler()
{
    const auto& dx12 = DX12::Instance();

    // GPU周波数取得
    if (dx12.getCommandQueue())
    {
        dx12.getCommandQueue()->GetTimestampFrequency(&m_gpuFreq);
    }

    auto* device = dx12.getDevice();
    if (device)
    {
        D3D12_QUERY_HEAP_DESC heapDesc = {};
        heapDesc.Count = QUERY_COUNT_PER_FRAME;
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

        D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * QUERY_COUNT_PER_FRAME);
        auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

        for (auto& frame : m_frames)
        {
            HRESULT hr = device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(frame.queryHeap.GetAddressOf()));
            LOG_HR(hr, "CpuGpuProfiler: CreateQueryHeap failed");

            hr = device->CreateCommittedResource(
                &readbackHeap,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(frame.readbackBuffer.GetAddressOf()));
            LOG_HR(hr, "CpuGpuProfiler: CreateCommittedResource(readback) failed");

            clearFrame(frame);
        }

        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
        {
            const LUID targetLuid = device->GetAdapterLuid();
            for (UINT i = 0;; ++i)
            {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
                if (factory->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND)
                {
                    break;
                }

                DXGI_ADAPTER_DESC1 desc = {};
                if (FAILED(adapter->GetDesc1(&desc)))
                {
                    continue;
                }

                if (std::memcmp(&desc.AdapterLuid, &targetLuid, sizeof(LUID)) == 0)
                {
                    adapter.As(&m_adapter3);
                    break;
                }
            }
        }
    }

    m_cpuHistory.resize(HistoryLength, 0.0f);
    m_gpuHistory.resize(HistoryLength, 0.0f);
    m_fpsHistory.resize(HistoryLength, 0.0f);
    m_cpuMemoryHistory.resize(HistoryLength, 0.0f);
    m_gpuMemoryHistory.resize(HistoryLength, 0.0f);
}

void CpuGpuProfiler::beginFrame(ID3D12GraphicsCommandList* cmd)
{
    consumeCompletedFrame();
    sampleMemoryStats();

    auto& frame = m_frames[m_currentFrameIndex];
    clearFrame(frame);

    // CPU 計測開始
    m_cpuStart = std::chrono::high_resolution_clock::now();

    // GPU 計測開始
    UINT startQuery = UINT_MAX;
    if (cmd && allocateQuery(frame, startQuery))
    {
        frame.frameStartQuery = startQuery;
        cmd->EndQuery(frame.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frame.frameStartQuery);
    }
}

void CpuGpuProfiler::endFrame(ID3D12GraphicsCommandList* cmd)
{
    auto& frame = m_frames[m_currentFrameIndex];

    {
        std::lock_guard<std::mutex> lock(m_scopeMutex);
        frame.pendingScopes.clear();
    }

    // GPU 計測終了
    if (cmd)
    {
        UINT endQuery = UINT_MAX;
        if (allocateQuery(frame, endQuery))
        {
            frame.frameEndQuery = endQuery;
            cmd->EndQuery(frame.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frame.frameEndQuery);
        }

        // 結果コピー
        if (frame.queryCount > 0)
        {
            cmd->ResolveQueryData(
                frame.queryHeap.Get(),
                D3D12_QUERY_TYPE_TIMESTAMP,
                0,
                frame.queryCount,
                frame.readbackBuffer.Get(),
                0);
        }
    }

    // CPU 計測終了
    auto cpuEnd = std::chrono::high_resolution_clock::now();
    m_cpuTimeMs = std::chrono::duration<float, std::milli>(cpuEnd - m_cpuStart).count();

    // 履歴更新
    pushHistory(m_cpuHistory, m_cpuTimeMs);

    m_lastCompletedFrameIndex = m_currentFrameIndex;
    m_hasCompletedFrame = true;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % BUFFERED_FRAMES;
}

CpuGpuProfiler::GpuScopeToken CpuGpuProfiler::beginGpuScope(ID3D12GraphicsCommandList* cmd, const char* name)
{
    GpuScopeToken token{};

    if (!cmd || !name || name[0] == '\0')
    {
        return token;
    }

    auto& frame = m_frames[m_currentFrameIndex];
    std::lock_guard<std::mutex> lock(m_scopeMutex);

    UINT startQuery = UINT_MAX;
    if (!allocateQuery(frame, startQuery))
    {
        return token;
    }

    cmd->EndQuery(frame.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startQuery);

    token.id = m_nextScopeToken++;
    token.valid = true;
    frame.pendingScopes.emplace(token.id, PendingScope{ name, startQuery });
    return token;
}

void CpuGpuProfiler::endGpuScope(ID3D12GraphicsCommandList* cmd, GpuScopeToken token)
{
    if (!token.valid || !cmd)
    {
        return;
    }

    auto& frame = m_frames[m_currentFrameIndex];
    std::lock_guard<std::mutex> lock(m_scopeMutex);

    auto it = frame.pendingScopes.find(token.id);
    if (it == frame.pendingScopes.end())
    {
        return;
    }

    UINT endQuery = UINT_MAX;
    if (!allocateQuery(frame, endQuery))
    {
        frame.pendingScopes.erase(it);
        return;
    }

    cmd->EndQuery(frame.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endQuery);
    frame.finishedScopes.push_back(FinishedScope{ std::move(it->second.name), it->second.startQuery, endQuery });
    frame.pendingScopes.erase(it);
}

void CpuGpuProfiler::recordFps(float fps)
{
    pushHistory(m_fpsHistory, fps);
}

void CpuGpuProfiler::pushHistory(std::vector<float>& history, float value)
{
    if (history.empty())
    {
        return;
    }

    history.erase(history.begin());
    history.push_back(value);
}

bool CpuGpuProfiler::allocateQuery(QueryFrameData& frame, UINT& outQueryIndex)
{
    if (frame.queryCount >= QUERY_COUNT_PER_FRAME)
    {
        return false;
    }

    outQueryIndex = frame.queryCount++;
    return true;
}

void CpuGpuProfiler::consumeCompletedFrame()
{
    if (!m_hasCompletedFrame)
    {
        return;
    }

    auto& frame = m_frames[m_lastCompletedFrameIndex];
    if (!frame.readbackBuffer || frame.queryCount == 0)
    {
        m_gpuScopeSamples.clear();
        pushHistory(m_gpuHistory, 0.0f);
        return;
    }

    void* mapped = nullptr;
    if (FAILED(frame.readbackBuffer->Map(0, nullptr, &mapped)) || !mapped)
    {
        m_gpuScopeSamples.clear();
        return;
    }

    std::vector<UINT64> timestamps(frame.queryCount, 0);
    std::memcpy(timestamps.data(), mapped, sizeof(UINT64) * frame.queryCount);
    frame.readbackBuffer->Unmap(0, nullptr);

    m_gpuScopeSamples.clear();

    if (m_gpuFreq > 0 && isValidPair(frame.frameStartQuery, frame.frameEndQuery) && frame.frameEndQuery < timestamps.size())
    {
        const UINT64 startTick = timestamps[frame.frameStartQuery];
        const UINT64 endTick = timestamps[frame.frameEndQuery];
        if (endTick >= startTick)
        {
            m_gpuTimeMs = static_cast<float>(endTick - startTick) * 1000.0f / static_cast<float>(m_gpuFreq);
        }
    }

    for (const auto& scope : frame.finishedScopes)
    {
        if (m_gpuFreq == 0)
        {
            break;
        }

        if (!isValidPair(scope.startQuery, scope.endQuery))
        {
            continue;
        }
        if (scope.endQuery >= timestamps.size())
        {
            continue;
        }

        const UINT64 startTick = timestamps[scope.startQuery];
        const UINT64 endTick = timestamps[scope.endQuery];
        if (endTick < startTick)
        {
            continue;
        }

        GpuScopeSample sample;
        sample.name = scope.name;
        sample.gpuMs = static_cast<float>(endTick - startTick) * 1000.0f / static_cast<float>(m_gpuFreq);
        m_gpuScopeSamples.push_back(std::move(sample));
    }

    std::sort(m_gpuScopeSamples.begin(), m_gpuScopeSamples.end(),
        [](const GpuScopeSample& a, const GpuScopeSample& b)
        {
            return a.gpuMs > b.gpuMs;
        });

    if (m_gpuScopeSamples.size() > MaxScopeSamples)
    {
        m_gpuScopeSamples.resize(MaxScopeSamples);
    }

    pushHistory(m_gpuHistory, m_gpuTimeMs);
}

void CpuGpuProfiler::sampleMemoryStats()
{
    PROCESS_MEMORY_COUNTERS_EX mem = {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem), sizeof(mem)))
    {
        m_cpuMemoryMB = static_cast<float>(mem.WorkingSetSize) / (1024.0f * 1024.0f);
    }

    if (m_adapter3)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
        if (SUCCEEDED(m_adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
        {
            m_gpuMemoryMB = static_cast<float>(info.CurrentUsage) / (1024.0f * 1024.0f);
        }
    }

    pushHistory(m_cpuMemoryHistory, m_cpuMemoryMB);
    pushHistory(m_gpuMemoryHistory, m_gpuMemoryMB);
}

void CpuGpuProfiler::clearFrame(QueryFrameData& frame)
{
    frame.queryCount = 0;
    frame.frameStartQuery = UINT_MAX;
    frame.frameEndQuery = UINT_MAX;
    frame.pendingScopes.clear();
    frame.finishedScopes.clear();
}