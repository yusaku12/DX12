#include "pch.h"

void PostEffectRenderTargets::initialize()
{
    UINT width = DX12::Instance().getScreenWidth();
    UINT height = DX12::Instance().getScreenHeight();
    DXGI_FORMAT format = DX12::Instance().getBackBufferFormat();

    //! ピンポン用 RTV ヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = PING_PONG_COUNT;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    auto* device = DX12::Instance().getDevice();
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf()));
    LOG_HR(hr, "Failed to create PostEffect RTV heap");

    createResources(width, height, format);
    m_initialized = true;

    LOG_INFO("PostEffectRenderTargets initialized");
}

void PostEffectRenderTargets::resize(UINT width, UINT height)
{
    if (!m_initialized) return;
    if (width == 0 || height == 0) return;

    releaseResources();
    createResources(width, height, DX12::Instance().getBackBufferFormat());
}

void PostEffectRenderTargets::createResources(UINT width, UINT height, DXGI_FORMAT format)
{
    auto* device = DX12::Instance().getDevice();
    UINT rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (int i = 0; i < PING_PONG_COUNT; ++i)
    {
        //! レンダーターゲットリソース作成
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = format;
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(m_renderTargets[i].GetAddressOf())
        );
        LOG_HR(hr, "Failed to create PostEffect RT");

        // RTV 作成
        m_rtvHandles[i] = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_rtvHandles[i].ptr += static_cast<SIZE_T>(i) * rtvIncrement;
        device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, m_rtvHandles[i]);

        // SRV 作成
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (m_srvIndices[i] == UINT_MAX)
        {
            m_srvIndices[i] = DescriptorHeapManager::Instance().allocateRange();
        }
        auto cpuHandle = DescriptorHeapManager::Instance().getCPUHandle(m_srvIndices[i]);
        device->CreateShaderResourceView(m_renderTargets[i].Get(), &srvDesc, cpuHandle);

        m_states[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void PostEffectRenderTargets::releaseResources()
{
    for (int i = 0; i < PING_PONG_COUNT; ++i)
    {
        m_renderTargets[i].Reset();
        // SRV インデックスは再利用するため解放しない
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE PostEffectRenderTargets::getCurrentRTV() const
{
    return m_rtvHandles[m_writeIndex];
}

void PostEffectRenderTargets::transitionWriteToRenderTarget(ID3D12GraphicsCommandList* cmd)
{
    if (m_states[m_writeIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_writeIndex].Get(),
            m_states[m_writeIndex],
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);
        m_states[m_writeIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
}

void PostEffectRenderTargets::transitionWriteToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (m_states[m_writeIndex] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_writeIndex].Get(),
            m_states[m_writeIndex],
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);
        m_states[m_writeIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void PostEffectRenderTargets::swap()
{
    m_inputSrvIndex = m_srvIndices[m_writeIndex];
    m_writeIndex = (m_writeIndex + 1) % PING_PONG_COUNT;
}

void PostEffectRenderTargets::reset(UINT sceneSrvIndex)
{
    m_inputSrvIndex = sceneSrvIndex;
    m_writeIndex = 0;
}