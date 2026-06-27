#include "pch.h"
#include "BloomEffect.h"

void BloomEffect::initialize()
{
    // PSO 登録
    m_psoPrefilter = registerPSO(ShaderID::BloomPrefilterPS, RootSignatureType::PostEffect);
    m_psoDownsample = registerPSO(ShaderID::BloomDownsamplePS, RootSignatureType::PostEffect);
    m_psoUpsample = registerPSO(ShaderID::BloomUpsamplePS, RootSignatureType::PostEffect);
    m_psoComposite = registerPSO(ShaderID::BloomCompositePS, RootSignatureType::BloomComposite);

    // 定数バッファ作成
    m_cbPrefilter = std::make_unique<ConstantBuffer<PrefilterCBuffer>>();
    m_cbBloom = std::make_unique<ConstantBuffer<BloomCBuffer>>();
    m_cbComposite = std::make_unique<ConstantBuffer<CompositeCBuffer>>();

    // 中間 RT 作成
    UINT w = DX12::Instance().getScreenWidth();
    UINT h = DX12::Instance().getScreenHeight();
    createMipRenderTargets(w, h);

    LOG_INFO("BloomEffect initialized (pyramid mode, {} levels)", MIP_COUNT);
}

void BloomEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    if (!cmd || inputSrvIndex == UINT_MAX)
    {
        return;
    }

    // Prefilter: フル解像度 → mip[0]（1/2）
    passPrefilter(cmd, inputSrvIndex);

    // Downsample ピラミッド
    for (int i = 0; i < MIP_COUNT - 1; ++i)
    {
        passDownsample(cmd, i, i + 1);
    }

    // Upsample ピラミッド
    for (int i = MIP_COUNT - 1; i > 0; --i)
    {
        passUpsample(cmd, i, i - 1);
    }

    // Composite: mip[0] を元シーンに加算合成
    passComposite(cmd, inputSrvIndex, m_mipSRV[0]);
}

void BloomEffect::inspectGUI()
{
    ImGui::SliderFloat("Threshold", &m_params.threshold, 0.0f, 3.0f);
    ImGui::SliderFloat("Knee", &m_params.knee, 0.0f, 1.0f);
    ImGui::SliderFloat("Intensity", &m_params.intensity, 0.0f, 5.0f);
    ImGui::SliderFloat("Scatter", &m_params.scatter, 0.0f, 1.0f);
}

void BloomEffect::createMipRenderTargets(UINT width, UINT height)
{
    auto* device = DX12::Instance().getDevice();
    DXGI_FORMAT fmt = DX12::Instance().getBackBufferFormat();

    // RTV ヒープ
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = MIP_COUNT;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf()));
    LOG_HR(hr, "BloomEffect: Failed to create RTV heap");

    UINT rtvStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (int i = 0; i < MIP_COUNT; ++i)
    {
        m_mipWidth[i] = std::max(1u, width >> (i + 1));
        m_mipHeight[i] = std::max(1u, height >> (i + 1));

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_mipWidth[i];
        desc.Height = m_mipHeight[i];
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = fmt;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear = {};
        clear.Format = fmt;
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clear, IID_PPV_ARGS(m_mipRT[i].GetAddressOf()));
        LOG_HR(hr, "BloomEffect: Failed to create mip RT[{}]", i);

        // RTV
        m_mipRTV[i] = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_mipRTV[i].ptr += static_cast<SIZE_T>(i) * rtvStep;
        device->CreateRenderTargetView(m_mipRT[i].Get(), nullptr, m_mipRTV[i]);

        // SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = fmt;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        m_mipSRV[i] = DescriptorHeapManager::Instance().allocateRange();
        device->CreateShaderResourceView(
            m_mipRT[i].Get(), &srvDesc,
            DescriptorHeapManager::Instance().getCPUHandle(m_mipSRV[i]));
        DescriptorHeapManager::Instance().syncToVisible(m_mipSRV[i]);

        m_mipState[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void BloomEffect::transitionToRT(ID3D12GraphicsCommandList* cmd, int idx)
{
    if (!cmd || idx < 0 || idx >= MIP_COUNT || !m_mipRT[idx]) return;
    if (m_mipState[idx] == D3D12_RESOURCE_STATE_RENDER_TARGET) return;
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_mipRT[idx].Get(), m_mipState[idx], D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &b);
    m_mipState[idx] = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void BloomEffect::transitionToSRV(ID3D12GraphicsCommandList* cmd, int idx)
{
    if (!cmd || idx < 0 || idx >= MIP_COUNT || !m_mipRT[idx]) return;
    if (m_mipState[idx] == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) return;
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_mipRT[idx].Get(), m_mipState[idx], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &b);
    m_mipState[idx] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

static void setViewport(ID3D12GraphicsCommandList* cmd, UINT w, UINT h)
{
    D3D12_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0.0f, 1.0f };
    D3D12_RECT     sc = { 0, 0, (LONG)w,  (LONG)h };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
}

void BloomEffect::passPrefilter(ID3D12GraphicsCommandList* cmd, UINT sceneSrvIndex)
{
    if (!cmd || sceneSrvIndex == UINT_MAX) return;

    transitionToRT(cmd, 0);
    setViewport(cmd, m_mipWidth[0], m_mipHeight[0]);
    cmd->OMSetRenderTargets(1, &m_mipRTV[0], FALSE, nullptr);

    PrefilterCBuffer cb{};
    cb.threshold = m_params.threshold;
    cb.knee = m_params.knee;
    cb.texelSize = { 1.0f / m_mipWidth[0], 1.0f / m_mipHeight[0] };
    m_cbPrefilter->update(cb);

    applyPSO(m_psoPrefilter, cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cbPrefilter->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(sceneSrvIndex));
    drawFullscreenTriangle(cmd);

    transitionToSRV(cmd, 0);
}

void BloomEffect::passDownsample(ID3D12GraphicsCommandList* cmd, int srcIdx, int dstIdx)
{
    if (!cmd || srcIdx < 0 || srcIdx >= MIP_COUNT || dstIdx < 0 || dstIdx >= MIP_COUNT) return;

    transitionToRT(cmd, dstIdx);
    setViewport(cmd, m_mipWidth[dstIdx], m_mipHeight[dstIdx]);
    cmd->OMSetRenderTargets(1, &m_mipRTV[dstIdx], FALSE, nullptr);

    BloomCBuffer cb{};
    cb.texelSize = { 1.0f / m_mipWidth[srcIdx], 1.0f / m_mipHeight[srcIdx] };
    cb.scatter = m_params.scatter;
    m_cbBloom->update(cb);

    applyPSO(m_psoDownsample, cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cbBloom->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(m_mipSRV[srcIdx]));
    drawFullscreenTriangle(cmd);

    transitionToSRV(cmd, dstIdx);
}

void BloomEffect::passUpsample(ID3D12GraphicsCommandList* cmd, int srcIdx, int dstIdx)
{
    if (!cmd || srcIdx < 0 || srcIdx >= MIP_COUNT || dstIdx < 0 || dstIdx >= MIP_COUNT) return;

    transitionToRT(cmd, dstIdx);
    setViewport(cmd, m_mipWidth[dstIdx], m_mipHeight[dstIdx]);
    cmd->OMSetRenderTargets(1, &m_mipRTV[dstIdx], FALSE, nullptr);

    BloomCBuffer cb{};
    cb.texelSize = { 1.0f / m_mipWidth[srcIdx], 1.0f / m_mipHeight[srcIdx] };
    cb.scatter = m_params.scatter;
    m_cbBloom->update(cb);

    applyPSO(m_psoUpsample, cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cbBloom->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(m_mipSRV[srcIdx]));
    drawFullscreenTriangle(cmd);

    transitionToSRV(cmd, dstIdx);
}

void BloomEffect::passComposite(ID3D12GraphicsCommandList* cmd,
    UINT sceneSrvIndex, UINT bloomSrvIndex)
{
    if (!cmd || sceneSrvIndex == UINT_MAX || bloomSrvIndex == UINT_MAX) return;

    // execute() 側で RT 遷移済み → OMSetRenderTargets だけ再セットする
    auto rtv = PostEffectRenderTargets::Instance().getCurrentRTV();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    UINT w = DX12::Instance().getScreenWidth();
    UINT h = DX12::Instance().getScreenHeight();
    setViewport(cmd, w, h);

    CompositeCBuffer cb{};
    cb.intensity = m_params.intensity;
    m_cbComposite->update(cb);

    applyPSO(m_psoComposite, cmd);
    cmd->SetGraphicsRootConstantBufferView(0, m_cbComposite->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(sceneSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(bloomSrvIndex));
    drawFullscreenTriangle(cmd);
}