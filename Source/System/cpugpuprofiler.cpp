#include "pch.h"
#include "cpugpuprofiler.h"

CpuGpuProfiler::CpuGpuProfiler()
{
    const auto& dx12 = DX12::Instance();

    //! GPU周波数取得
    dx12.getCommandQueue()->GetTimestampFrequency(&m_gpuFreq);

    //! QueryHeap 作成
    D3D12_QUERY_HEAP_DESC desc = {};
    desc.Count = QUERYCOUNT;
    desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    dx12.getDevice()->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_queryHeap));

    //! 結果格納バッファ
    D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * QUERYCOUNT);
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    dx12.getDevice()->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_queryBuffer)
    );

    m_cpuHistory.resize(120);
    m_gpuHistory.resize(120);
    m_fpsHistory.resize(120);
}

void CpuGpuProfiler::beginFrame(ID3D12GraphicsCommandList* cmd)
{
    //! CPU 計測開始
    m_cpuStart = std::chrono::high_resolution_clock::now();

    //! GPU 計測開始
    cmd->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
}

void CpuGpuProfiler::endFrame(ID3D12GraphicsCommandList* cmd)
{
    //! GPU 計測終了
    cmd->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    // 結果コピー
    cmd->ResolveQueryData(
        m_queryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0,
        QUERYCOUNT,
        m_queryBuffer.Get(),
        0
    );

    //! CPU 計測終了
    auto cpuEnd = std::chrono::high_resolution_clock::now();
    m_cpuTimeMs = std::chrono::duration<float, std::milli>(cpuEnd - m_cpuStart).count();

    //! 履歴更新
    m_cpuHistory.erase(m_cpuHistory.begin());
    m_cpuHistory.push_back(m_cpuTimeMs);

    //! GPU データ取得
    UINT64 result[QUERYCOUNT] = {};
    void* mapped = nullptr;

    if (SUCCEEDED(m_queryBuffer->Map(0, nullptr, &mapped)))
    {
        memcpy(result, mapped, sizeof(result));
        m_queryBuffer->Unmap(0, nullptr);

        UINT64 start = result[0];
        UINT64 end = result[1];

        m_gpuTimeMs = float(end - start) * 1000.0f / m_gpuFreq;
    }

    m_gpuHistory.erase(m_gpuHistory.begin());
    m_gpuHistory.push_back(m_gpuTimeMs);
}

void CpuGpuProfiler::recordFps(float fps)
{
    m_fpsHistory.erase(m_fpsHistory.begin());
    m_fpsHistory.push_back(fps);
}