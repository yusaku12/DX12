#include "pch.h"

#include <dx12/ffx_api_dx12.hpp>
#include <ffx_upscale.hpp>

#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "PostEffect/PostEffectRenderTargets.h"
#include "Render/FidelityFXUpscaler.h"
#include "Render/GBufferRenderTargets.h"
#include "System/TimeManager.h"

namespace
{
    constexpr FfxApiUpscaleQualityMode kQualityModes[] =
    {
        FFX_UPSCALE_QUALITY_MODE_NATIVEAA,
        FFX_UPSCALE_QUALITY_MODE_QUALITY,
        FFX_UPSCALE_QUALITY_MODE_BALANCED,
        FFX_UPSCALE_QUALITY_MODE_PERFORMANCE,
        FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE
    };

    constexpr const char* kQualityNames[] =
    {
        "Native AA",
        "Quality",
        "Balanced",
        "Performance",
        "Ultra Performance"
    };
}

void FidelityFXUpscaler::initialize()
{
    if (!m_enabled || m_context)
    {
        return;
    }

    if (createContext())
    {
        applyQualityMode();
        LOG_INFO("AMD FidelityFX SDK upscaler initialized");
    }
}

void FidelityFXUpscaler::shutdown()
{
    destroyContext();
    m_output.Reset();

    if (m_outputSrvIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_outputSrvIndex);
        m_outputSrvIndex = UINT_MAX;
    }
}

bool FidelityFXUpscaler::createContext()
{
    auto* device = DX12::Instance().getDevice();
    if (!device)
    {
        return false;
    }

    ffx::CreateContextDescUpscale upscaleDesc{};
    upscaleDesc.flags = FFX_UPSCALE_ENABLE_AUTO_EXPOSURE
        | FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION
        | FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION
        | FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE;
#ifdef _DEBUG
    upscaleDesc.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
#endif
    upscaleDesc.maxRenderSize = {
        static_cast<uint32_t>(DX12::Instance().getDisplayWidth()),
        static_cast<uint32_t>(DX12::Instance().getDisplayHeight()) };
    upscaleDesc.maxUpscaleSize = upscaleDesc.maxRenderSize;

    ffx::CreateBackendDX12Desc backendDesc{};
    backendDesc.device = device;

    const auto result = ffx::CreateContext(m_context, nullptr, upscaleDesc, backendDesc);
    if (!!result)
    {
        return true;
    }

    LOG_ERROR(std::format("FidelityFX: context creation failed ({})", static_cast<uint32_t>(result)));
    m_context = nullptr;
    m_enabled = false;
    return false;
}

void FidelityFXUpscaler::destroyContext()
{
    if (!m_context)
    {
        return;
    }

    const auto result = ffx::DestroyContext(m_context);
    if (!!result)
    {
        m_context = nullptr;
        return;
    }

    LOG_ERROR(std::format("FidelityFX: context destruction failed ({})", static_cast<uint32_t>(result)));
    m_context = nullptr;
}

void FidelityFXUpscaler::beginFrame()
{
    m_jitter = Vector2::Zero;
    if (!isEnabled())
    {
        return;
    }

    const uint32_t renderWidth = static_cast<uint32_t>(DX12::Instance().getScreenWidth());
    const uint32_t displayWidth = static_cast<uint32_t>(DX12::Instance().getDisplayWidth());

    ffx::QueryDescUpscaleGetJitterPhaseCount phaseQuery{};
    phaseQuery.renderWidth = renderWidth;
    phaseQuery.displayWidth = displayWidth;
    phaseQuery.pOutPhaseCount = &m_jitterPhaseCount;
    if (!ffx::Query(m_context, phaseQuery))
    {
        m_jitterPhaseCount = 1;
    }

    m_jitterPhaseCount = std::max(1, m_jitterPhaseCount);
    m_jitterIndex %= m_jitterPhaseCount;

    ffx::QueryDescUpscaleGetJitterOffset jitterQuery{};
    jitterQuery.index = m_jitterIndex++;
    jitterQuery.phaseCount = m_jitterPhaseCount;
    jitterQuery.pOutX = &m_jitter.x;
    jitterQuery.pOutY = &m_jitter.y;
    if (!ffx::Query(m_context, jitterQuery))
    {
        m_jitter = Vector2::Zero;
    }
}

bool FidelityFXUpscaler::ensureOutput(UINT width, UINT height)
{
    if (m_output && m_outputWidth == width && m_outputHeight == height)
    {
        return true;
    }

    if (m_output)
    {
        DX12::Instance().safeGPUWait();
        m_output.Reset();
        destroyContext();
        if (!createContext())
        {
            return false;
        }
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DX12::Instance().getBackBufferFormat();
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    const HRESULT hr = DX12::Instance().getDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(m_output.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        LOG_ERROR("FidelityFX: output resource creation failed (0x{:08X})", static_cast<uint32_t>(hr));
        return false;
    }

    m_output->SetName(L"FidelityFX Upscaled Output");
    m_outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_outputWidth = width;
    m_outputHeight = height;
    m_previousRenderWidth = 0;
    m_previousRenderHeight = 0;

    if (m_outputSrvIndex == UINT_MAX)
    {
        m_outputSrvIndex = DescriptorHeapManager::Instance().allocateRange();
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    DX12::Instance().getDevice()->CreateShaderResourceView(
        m_output.Get(),
        &srvDesc,
        DescriptorHeapManager::Instance().getCPUHandle(m_outputSrvIndex));
    DescriptorHeapManager::Instance().syncToVisible(m_outputSrvIndex);
    return true;
}

UINT FidelityFXUpscaler::execute(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    if (!isEnabled() || !cmd)
    {
        return inputSrvIndex;
    }

    auto* color = PostEffectRenderTargets::Instance().getResourceForSrv(inputSrvIndex);
    auto* depth = DX12::Instance().getDepthResource();
    auto* motionVectors = GBufferRenderTargets::Instance().getResource(3);
    auto* camera = CameraManager::Instance().getMainCamera();
    const UINT renderWidth = static_cast<UINT>(DX12::Instance().getScreenWidth());
    const UINT renderHeight = static_cast<UINT>(DX12::Instance().getScreenHeight());
    const UINT outputWidth = static_cast<UINT>(DX12::Instance().getDisplayWidth());
    const UINT outputHeight = static_cast<UINT>(DX12::Instance().getDisplayHeight());

    if (!color || !depth || !motionVectors || !camera || !ensureOutput(outputWidth, outputHeight))
    {
        return inputSrvIndex;
    }

    DX12::Instance().transitionDepthToSRV();

    ffx::DispatchDescUpscale dispatch{};
    dispatch.commandList = cmd;
    dispatch.color = ffxApiGetResourceDX12(color, FFX_API_RESOURCE_STATE_PIXEL_READ);
    dispatch.depth = ffxApiGetResourceDX12(depth, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatch.depth.description.format = FFX_API_SURFACE_FORMAT_R32_FLOAT;
    dispatch.motionVectors = ffxApiGetResourceDX12(motionVectors, FFX_API_RESOURCE_STATE_PIXEL_READ);
    dispatch.output = ffxApiGetResourceDX12(
        m_output.Get(),
        FFX_API_RESOURCE_STATE_UNORDERED_ACCESS,
        FFX_API_RESOURCE_USAGE_UAV);
    dispatch.jitterOffset = { m_jitter.x, m_jitter.y };
    dispatch.motionVectorScale = {
        -static_cast<float>(renderWidth),
        -static_cast<float>(renderHeight) };
    dispatch.renderSize = { renderWidth, renderHeight };
    dispatch.upscaleSize = { outputWidth, outputHeight };
    dispatch.enableSharpening = m_enableSharpening;
    dispatch.sharpness = std::clamp(m_sharpness, 0.0f, 1.0f);
    dispatch.frameTimeDelta = std::max(0.01f, TimeManager::Instance().getUnscaledDeltaTime() * 1000.0f);
    dispatch.preExposure = 1.0f;
    dispatch.reset = renderWidth != m_previousRenderWidth || renderHeight != m_previousRenderHeight;
    dispatch.cameraNear = camera->getNear();
    dispatch.cameraFar = camera->getFar();
    dispatch.cameraFovAngleVertical = camera->getFov();
    dispatch.viewSpaceToMetersFactor = 1.0f;
    dispatch.flags = FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_output.Get(),
        m_outputState,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->ResourceBarrier(1, &barrier);
    m_outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    const auto result = ffx::Dispatch(m_context, dispatch);
    // FidelityFX は独自のディスクリプタヒープを設定するため、後続の最終合成と ImGui 用に復元する。
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    DX12::Instance().transitionDepthToWrite();
    if (!result)
    {
        LOG_ERROR(std::format("FidelityFX: upscale dispatch failed ({})", static_cast<uint32_t>(result)));
        return inputSrvIndex;
    }

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_output.Get(),
        m_outputState,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
    m_outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_previousRenderWidth = renderWidth;
    m_previousRenderHeight = renderHeight;
    return m_outputSrvIndex;
}

void FidelityFXUpscaler::applyQualityMode()
{
    const auto mode = kQualityModes[std::clamp(m_qualityMode, 0, static_cast<int>(std::size(kQualityModes)) - 1)];
    float ratio = 1.0f;
    ffx::QueryDescUpscaleGetUpscaleRatioFromQualityMode query{};
    query.qualityMode = mode;
    query.pOutUpscaleRatio = &ratio;
    if (!m_context || !ffx::Query(m_context, query))
    {
        return;
    }

    DX12::Instance().setRenderScale(1.0f / std::max(1.0f, ratio));
    m_previousRenderWidth = 0;
    m_previousRenderHeight = 0;
}

void FidelityFXUpscaler::renderDebugContents()
{
    ImGui::SeparatorText("AMD FidelityFX FSR");
    if (ImGui::Checkbox("Enable FSR Upscaler", &m_enabled))
    {
        m_jitterIndex = 0;
        m_previousRenderWidth = 0;
        m_previousRenderHeight = 0;
        if (m_enabled)
        {
            if (!m_context)
            {
                createContext();
            }
            applyQualityMode();
        }
        else
        {
            DX12::Instance().setRenderScale(1.0f);
        }
    }

    if (!m_enabled)
    {
        return;
    }

    if (ImGui::Combo("FSR Quality", &m_qualityMode, kQualityNames, IM_ARRAYSIZE(kQualityNames)))
    {
        applyQualityMode();
    }
    ImGui::Checkbox("FSR Sharpening", &m_enableSharpening);
    if (m_enableSharpening)
    {
        ImGui::SliderFloat("FSR Sharpness", &m_sharpness, 0.0f, 1.0f, "%.2f");
    }
    ImGui::Text("Output Size: %d x %d", DX12::Instance().getDisplayWidth(), DX12::Instance().getDisplayHeight());
}