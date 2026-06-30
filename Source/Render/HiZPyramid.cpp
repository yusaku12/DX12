#include "pch.h"
#include "HiZPyramid.h"

namespace
{
    constexpr D3D12_RESOURCE_STATES kSrvState =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    UINT calcMipCount(UINT width, UINT height)
    {
        UINT mips = 1;
        UINT size = std::max(width, height);
        while (size > 1)
        {
            size = std::max(1u, size / 2u);
            ++mips;
        }
        return mips;
    }

    UINT divUp(UINT x, UINT y)
    {
        return (x + y - 1) / y;
    }
}

void HiZPyramid::ensureInitialized()
{
    if (m_initialized)
    {
        return;
    }

    auto* device = DX12::Instance().getDevice();
    if (!device)
    {
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = RootSignatureManager::Instance().getRootSignature(RootSignatureType::HiZPyramidCompute);

    ID3DBlob* cs = ShaderManager::Instance().getShaderBlob(ShaderID::HiZDownsampleCS);
    if (!cs)
    {
        LOG_WARN("[HiZPyramid] Compute shader blob missing");
        return;
    }

    desc.CS.pShaderBytecode = cs->GetBufferPointer();
    desc.CS.BytecodeLength = cs->GetBufferSize();

    const HRESULT hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_pso.ReleaseAndGetAddressOf()));
    LOG_HR(hr, "[HiZPyramid] Failed to create compute PSO");

    m_paramsCB = DXMem::makeUnique<ConstantBuffer<HiZParams>>();
    m_initialized = true;
}

void HiZPyramid::releaseDescriptors()
{
    auto& heap = DescriptorHeapManager::Instance();

    for (UINT idx : m_mipSrvIndices)
    {
        if (idx != UINT_MAX)
        {
            heap.free(idx, 1);
        }
    }
    m_mipSrvIndices.clear();

    for (UINT idx : m_mipUavIndices)
    {
        if (idx != UINT_MAX)
        {
            heap.free(idx, 1);
        }
    }
    m_mipUavIndices.clear();
}

void HiZPyramid::recreateResourcesIfNeeded()
{
    const UINT width = static_cast<UINT>(std::max(1, DX12::Instance().getScreenWidth()));
    const UINT height = static_cast<UINT>(std::max(1, DX12::Instance().getScreenHeight()));
    const UINT mipCount = calcMipCount(width, height);

    if (m_hizTexture && m_width == width && m_height == height && m_mipCount == mipCount)
    {
        return;
    }

    releaseDescriptors();
    m_hizTexture.Reset();

    auto* device = DX12::Instance().getDevice();
    if (!device)
    {
        return;
    }

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = static_cast<UINT16>(mipCount);
    texDesc.Format = DXGI_FORMAT_R32_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    const HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        kSrvState,
        nullptr,
        IID_PPV_ARGS(m_hizTexture.ReleaseAndGetAddressOf()));
    LOG_HR(hr, "[HiZPyramid] Failed to create hiz texture");

    m_width = width;
    m_height = height;
    m_mipCount = mipCount;

    m_mipSrvIndices.resize(mipCount, UINT_MAX);
    m_mipUavIndices.resize(mipCount, UINT_MAX);

    for (UINT mip = 0; mip < mipCount; ++mip)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = mip;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        m_mipSrvIndices[mip] = DescriptorHeapManager::Instance().createSRV(m_hizTexture.Get(), srvDesc);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = mip;
        uavDesc.Texture2D.PlaneSlice = 0;
        m_mipUavIndices[mip] = DescriptorHeapManager::Instance().createUAV(m_hizTexture.Get(), nullptr, uavDesc);
    }
}

void HiZPyramid::build(ID3D12GraphicsCommandList* cmd)
{
    if (!m_enabled || !cmd)
    {
        return;
    }

    ensureInitialized();
    if (!m_initialized || !m_pso)
    {
        return;
    }

    recreateResourcesIfNeeded();
    if (!m_hizTexture || m_mipSrvIndices.empty() || m_mipUavIndices.empty())
    {
        return;
    }

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    CD3DX12_RESOURCE_BARRIER toUav = CD3DX12_RESOURCE_BARRIER::Transition(
        m_hizTexture.Get(),
        kSrvState,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    cmd->ResourceBarrier(1, &toUav);

    cmd->SetComputeRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::HiZPyramidCompute));
    cmd->SetPipelineState(m_pso.Get());

    m_lastDispatchCount = 0;

    // mip0: direct copy from depth SRV
    {
        HiZParams params{};
        params.srcWidth = m_width;
        params.srcHeight = m_height;
        params.isFirstPass = 1;
        m_paramsCB->update(params);

        cmd->SetComputeRootConstantBufferView(0, m_paramsCB->getGPUAddress());
        cmd->SetComputeRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(DX12::Instance().getDepthSrvIndex()));
        cmd->SetComputeRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_mipUavIndices[0]));

        cmd->Dispatch(divUp(m_width, 8), divUp(m_height, 8), 1);
        ++m_lastDispatchCount;

        CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_hizTexture.Get());
        cmd->ResourceBarrier(1, &uavBarrier);

        CD3DX12_RESOURCE_BARRIER mip0ToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_hizTexture.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            kSrvState,
            0);
        cmd->ResourceBarrier(1, &mip0ToSrv);
    }

    // mip1+: 2x2 max downsample from previous mip
    for (UINT mip = 1; mip < m_mipCount; ++mip)
    {
        const UINT srcWidth = std::max(1u, m_width >> (mip - 1));
        const UINT srcHeight = std::max(1u, m_height >> (mip - 1));
        const UINT dstWidth = std::max(1u, m_width >> mip);
        const UINT dstHeight = std::max(1u, m_height >> mip);

        HiZParams params{};
        params.srcWidth = srcWidth;
        params.srcHeight = srcHeight;
        params.isFirstPass = 0;
        m_paramsCB->update(params);

        cmd->SetComputeRootConstantBufferView(0, m_paramsCB->getGPUAddress());
        cmd->SetComputeRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(m_mipSrvIndices[mip - 1]));
        cmd->SetComputeRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_mipUavIndices[mip]));

        cmd->Dispatch(divUp(dstWidth, 8), divUp(dstHeight, 8), 1);
        ++m_lastDispatchCount;

        CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_hizTexture.Get());
        cmd->ResourceBarrier(1, &uavBarrier);

        CD3DX12_RESOURCE_BARRIER mipToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_hizTexture.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            kSrvState,
            mip);
        cmd->ResourceBarrier(1, &mipToSrv);
    }
}

void HiZPyramid::debugImgui()
{
    ImGui::Begin("Hi-Z Debug");

    renderDebugContents();

    ImGui::End();
}

void HiZPyramid::renderDebugContents()
{

    ImGui::Checkbox("Enable Hi-Z Build", &m_enabled);
    ImGui::Text("Resolution: %u x %u", m_width, m_height);
    ImGui::Text("Mip Count: %u", m_mipCount);
    ImGui::Text("Dispatches: %u", m_lastDispatchCount);
}