#include "pch.h"
#include "DescriptorHeapManager.h"

void DescriptorHeapManager::initialize(UINT maxCount)
{
    const auto device = DX12::Instance().getDevice();

    m_maxCount = maxCount;
    m_used.assign(maxCount, false);

    // shader-visible Descriptor Heap 作成
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = maxCount;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    LOG_HR(hr, "DescriptorHeap CreateDescriptorHeap failed");
    m_incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Non-shader-visible (Staging) Descriptor Heap 作成 (すべてのビューの一時的な作成宛先)
    D3D12_DESCRIPTOR_HEAP_DESC stagingDesc = desc;
    stagingDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(&stagingDesc, IID_PPV_ARGS(&m_stagingHeap));
    LOG_HR(hr, "DescriptorHeap CreateStagingDescriptorHeap failed");

    // 既存コードの互換性を保つため、インデックス0を予約している既存実装の挙動を尊重
    if (maxCount > 0)
        m_used[0] = true;
}

void DescriptorHeapManager::setDescriptorHeap()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd) return;

    ID3D12DescriptorHeap* heaps[] = { m_heap.Get() };
    cmd->SetDescriptorHeaps(_countof(heaps), heaps);
}

void DescriptorHeapManager::setDescriptorHeap(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    ID3D12DescriptorHeap* heaps[] = { m_heap.Get() };
    cmd->SetDescriptorHeaps(_countof(heaps), heaps);
}

UINT DescriptorHeapManager::createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    if (!device || !resource) return InvalidIndex;

    UINT index = allocateRange();
    if (index == InvalidIndex)
    {
        LOG_ERROR("DescriptorHeapManager::createSRV failed: descriptor heap exhausted (max=%u)", m_maxCount);
        return InvalidIndex;
    }

    // getCPUHandle が staging 側を返すため、自動的に非表示ヒープに作成されます
    D3D12_CPU_DESCRIPTOR_HANDLE stagingHandle = getCPUHandle(index);
    device->CreateShaderResourceView(resource, &desc, stagingHandle);

    // シェーダー可視ヒープに即時同期コピー
    syncToVisible(index);

    return index;
}

UINT DescriptorHeapManager::createCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    if (!device) return InvalidIndex;

    UINT index = allocateRange();
    if (index == InvalidIndex)
    {
        LOG_ERROR("DescriptorHeapManager::createCBV failed: descriptor heap exhausted (max=%u)", m_maxCount);
        return InvalidIndex;
    }

    // getCPUHandle が staging 側を返す
    D3D12_CPU_DESCRIPTOR_HANDLE stagingHandle = getCPUHandle(index);
    device->CreateConstantBufferView(&desc, stagingHandle);

    // シェーダー可視ヒープに即時同期コピー
    syncToVisible(index);

    return index;
}

UINT DescriptorHeapManager::createUAV(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
{
    const auto device = DX12::Instance().getDevice();
    if (!device || !resource) return InvalidIndex;

    UINT index = allocateRange();
    if (index == InvalidIndex)
    {
        LOG_ERROR("DescriptorHeapManager::createUAV failed: descriptor heap exhausted (max=%u)", m_maxCount);
        return InvalidIndex;
    }

    // getCPUHandle が staging 側を返す
    D3D12_CPU_DESCRIPTOR_HANDLE stagingHandle = getCPUHandle(index);
    device->CreateUnorderedAccessView(resource, counterResource, &desc, stagingHandle);

    // シェーダー可視ヒープに即時同期コピー
    syncToVisible(index);

    return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getGPUHandle(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = {};
    if (index == InvalidIndex || index >= m_maxCount || !m_heap)
        return h;

    h = m_heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * m_incrementSize;
    return h;
}

void DescriptorHeapManager::syncToVisible(UINT index, UINT count) const
{
    if (index == InvalidIndex || count == 0 || index >= m_maxCount)
    {
        return;
    }

    const auto device = DX12::Instance().getDevice();
    if (!device || !m_stagingHeap || !m_heap)
    {
        return;
    }

    const UINT copyCount = std::min(count, m_maxCount - index);
    if (copyCount == 0)
    {
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE srcStagingHandle = getCPUHandle(index);
    D3D12_CPU_DESCRIPTOR_HANDLE dstVisibleHandle = getCPUHandleVisible(index);
    device->CopyDescriptorsSimple(
        copyCount,
        dstVisibleHandle,
        srcStagingHandle,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCPUHandle(UINT index) const
{
    // ビュー構築時に安全に指定できるよう、常に Non-Shader-Visible ステージングヒープを返します。
    D3D12_CPU_DESCRIPTOR_HANDLE h = {};
    if (index == InvalidIndex || index >= m_maxCount || !m_stagingHeap)
        return h;

    h = m_stagingHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * m_incrementSize;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::getCPUHandleVisible(UINT index) const
{
    // 同期先の Shader-Visible ヒープです
    D3D12_CPU_DESCRIPTOR_HANDLE h = {};
    if (index == InvalidIndex || index >= m_maxCount || !m_heap)
        return h;

    h = m_heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * m_incrementSize;
    return h;
}

void DescriptorHeapManager::free(UINT index, UINT count)
{
    if (index == InvalidIndex || count == 0) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (index >= m_maxCount) return;
    for (UINT i = 0; i < count && (index + i) < m_maxCount; ++i)
    {
        m_used[index + i] = false;
    }
}

UINT DescriptorHeapManager::allocateRange(UINT count)
{
    if (count == 0) return InvalidIndex;
    std::lock_guard<std::mutex> lock(m_mutex);

    // 既存実装はインデックス1から探索している挙動に合わせる
    for (UINT start = 1; start + count <= m_maxCount; ++start)
    {
        bool freeBlock = true;
        for (UINT offset = 0; offset < count; ++offset)
        {
            if (m_used[start + offset])
            {
                freeBlock = false;
                break;
            }
        }

        if (freeBlock)
        {
            for (UINT offset = 0; offset < count; ++offset)
            {
                m_used[start + offset] = true;
            }
            return start;
        }
    }

    return UINT_MAX;
}

bool DescriptorHeapManager::copyDescriptorsRange(UINT dstIndex, const std::vector<UINT>& srcIndices)
{
    if (dstIndex == InvalidIndex || srcIndices.empty()) return false;

    const auto device = DX12::Instance().getDevice();
    if (!device) return false;

    for (size_t i = 0; i < srcIndices.size(); ++i)
    {
        UINT src = srcIndices[i];
        UINT dst = dstIndex + static_cast<UINT>(i);
        if (dst >= m_maxCount) break;

        // もしコピー元に InvalidIndex (テクスチャ未設定などでフォールバックや未ロード状態など)
        // などの無効インデックスが存在しても、処理を一括中断させず、安全にそのスロットだけを飛ばして次に進めます。
        if (src == InvalidIndex) continue;

        D3D12_CPU_DESCRIPTOR_HANDLE srcStagingHandle = getCPUHandle(src);
        D3D12_CPU_DESCRIPTOR_HANDLE dstShaderVisibleHandle = getCPUHandleVisible(dst);
        D3D12_CPU_DESCRIPTOR_HANDLE dstStagingHandle = getCPUHandle(dst);

        // 1. コピー元（ステージング）から同期先（シェーダー可視）へコピー
        device->CopyDescriptorsSimple(
            1,
            dstShaderVisibleHandle,
            srcStagingHandle,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );

        // 2. 将来的なコピーの整合性維持のため、同時にステージング側宛先インデックスにも同期保存
        device->CopyDescriptorsSimple(
            1,
            dstStagingHandle,
            srcStagingHandle,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );
    }

    return true;
}