#include "pch.h"

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

    DXGI_FORMAT formats[RenderTargetCount] =
    {
        BaseColorFormat,
        NormalRoughnessFormat,
        WorldPosAoFormat
    };

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = formats[i];
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = formats[i];

        if (i == 0)
        {
            clearValue.Color[0] = 0.0f;
            clearValue.Color[1] = 0.0f;
            clearValue.Color[2] = 0.0f;
            clearValue.Color[3] = 0.0f;
        }
        else if (i == 1)
        {
            clearValue.Color[0] = 0.5f;
            clearValue.Color[1] = 0.5f;
            clearValue.Color[2] = 1.0f;
            clearValue.Color[3] = 1.0f;
        }
        else
        {
            clearValue.Color[0] = 0.0f;
            clearValue.Color[1] = 0.0f;
            clearValue.Color[2] = 0.0f;
            clearValue.Color[3] = 1.0f;
        }

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
        m_rtvHandles[i] = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_rtvHandles[i].ptr += static_cast<SIZE_T>(i) * rtvIncrement;
        device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, m_rtvHandles[i]);

        // SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = formats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_srvBaseIndex + i);
        device->CreateShaderResourceView(m_renderTargets[i].Get(), &srvDesc, cpuHandle);

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

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        if (!m_renderTargets[i])
        {
            LOG_ERROR("GBufferRenderTargets: render target is null");
            continue;
        }

        if (m_states[i] != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[i].Get(),
                m_states[i],
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            cmd->ResourceBarrier(1, &barrier);
            m_states[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
    }
}

void GBufferRenderTargets::transitionToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    for (UINT i = 0; i < RenderTargetCount; ++i)
    {
        if (!m_renderTargets[i])
        {
            LOG_ERROR("GBufferRenderTargets: render target is null");
            continue;
        }

        if (m_states[i] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_renderTargets[i].Get(),
                m_states[i],
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd->ResourceBarrier(1, &barrier);
            m_states[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }
}

void GBufferRenderTargets::clear(ID3D12GraphicsCommandList* cmd)
{
    FLOAT clearBase[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    FLOAT clearNormal[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
    FLOAT clearWorld[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    cmd->ClearRenderTargetView(m_rtvHandles[0], clearBase, 0, nullptr);
    cmd->ClearRenderTargetView(m_rtvHandles[1], clearNormal, 0, nullptr);
    cmd->ClearRenderTargetView(m_rtvHandles[2], clearWorld, 0, nullptr);
}

void GBufferRenderTargets::setRenderTargets(ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const
{
    cmd->OMSetRenderTargets(RenderTargetCount, m_rtvHandles, FALSE, &dsvHandle);
}