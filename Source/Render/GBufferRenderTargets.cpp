#include "pch.h"

namespace
{
    constexpr DXGI_FORMAT kGBufferFormats[GBufferRenderTargets::RenderTargetCount] =
    {
        GBufferRenderTargets::BaseColorFormat,
        GBufferRenderTargets::NormalRoughnessFormat,
        GBufferRenderTargets::WorldPosAoFormat
    };

    constexpr FLOAT kGBufferClearColors[GBufferRenderTargets::RenderTargetCount][4] =
    {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.5f, 0.5f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f }
    };
}

void GBufferRenderTargets::initialize()
{
    UINT width = DX12::Instance().getScreenWidth();
    UINT height = DX12::Instance().getScreenHeight();

    //! RTV ヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = RenderTargetCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    auto* device = DX12::Instance().getDevice();
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf()));
    LOG_HR(hr, "Failed to create GBuffer RTV heap");

    createResources(width, height);
    m_initialized = true;

    LOG_INFO("GBufferRenderTargets initialized");
}

void GBufferRenderTargets::resize(UINT width, UINT height)
{
    if (!m_initialized) return;
    if (width == 0 || height == 0) return;

    releaseResources();
    createResources(width, height);
}

void GBufferRenderTargets::createResources(UINT width, UINT height)
{
    auto* device = DX12::Instance().getDevice();
    UINT rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (m_srvBaseIndex == UINT_MAX)
    {
        m_srvBaseIndex = DescriptorHeapManager::Instance().allocateRange(RenderTargetCount);
    }

    auto rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = kGBufferFormats[i];
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = kGBufferFormats[i];
        memcpy(clearValue.Color, kGBufferClearColors[i], sizeof(clearValue.Color));

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(m_renderTargets[i].GetAddressOf())
        );
        LOG_HR(hr, "Failed to create GBuffer RT");

        // RTV
        m_rtvHandles[i] = rtvHandle;
        device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, m_rtvHandles[i]);
        rtvHandle.ptr += rtvIncrement;

        // SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = kGBufferFormats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_srvBaseIndex + i);
        device->CreateShaderResourceView(m_renderTargets[i].Get(), &srvDesc, cpuHandle);
        DescriptorHeapManager::Instance().syncToVisible(m_srvBaseIndex + i);

        m_states[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void GBufferRenderTargets::releaseResources()
{
    for (auto& rt : m_renderTargets)
    {
        rt.Reset();
    }
}

void GBufferRenderTargets::transitionToRenderTarget(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    D3D12_RESOURCE_BARRIER barriers[RenderTargetCount];
    UINT barrierCount = 0;
    bool hasNull = false;

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        if (!m_renderTargets[i])
        {
            hasNull = true;
            continue;
        }

        if (m_states[i] != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[i].Get(),
                m_states[i],
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            m_states[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
    }

    if (hasNull)
        LOG_ERROR("GBufferRenderTargets: render target is null");

    if (barrierCount > 0)
        cmd->ResourceBarrier(barrierCount, barriers);
}

void GBufferRenderTargets::transitionToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    D3D12_RESOURCE_BARRIER barriers[RenderTargetCount];
    UINT barrierCount = 0;
    bool hasNull = false;

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        if (!m_renderTargets[i])
        {
            hasNull = true;
            continue;
        }

        if (m_states[i] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[i].Get(),
                m_states[i],
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_states[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }

    if (hasNull)
        LOG_ERROR("GBufferRenderTargets: render target is null");

    if (barrierCount > 0)
        cmd->ResourceBarrier(barrierCount, barriers);
}

void GBufferRenderTargets::clear(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        cmd->ClearRenderTargetView(m_rtvHandles[i], kGBufferClearColors[i], 0, nullptr);
    }
}

void GBufferRenderTargets::setRenderTargets(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const
{
    if (!cmd) return;

    cmd->OMSetRenderTargets(RenderTargetCount, m_rtvHandles, FALSE, &dsvHandle);
}

void GBufferRenderTargets::debugDrawImGui()
{
    ImGui::Begin("GBuffer Debug");

    renderDebugContents();

    ImGui::End();
}

void GBufferRenderTargets::renderDebugContents()
{

    if (m_srvBaseIndex == UINT_MAX)
    {
        ImGui::Text("GBuffer SRV is not initialized.");
        return;
    }

    const char* labels[] =
    {
        "BaseColor",
        "NormalRoughness",
        "WorldPosAo"
    };

    static constexpr float previewWidth = 256.0f;

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        const UINT srvIndex = getSrvIndex(i);
        if (srvIndex == UINT_MAX)
        {
            ImGui::Text("%s: SRV invalid", labels[i]);
            continue;
        }

        ImTextureID texID = (ImTextureID)DescriptorHeapManager::Instance().getGPUHandle(srvIndex).ptr;
        ImGui::Text("%s", labels[i]);
        ImGui::Image(texID, ImVec2(previewWidth, previewWidth));
        ImGui::Separator();
    }
}