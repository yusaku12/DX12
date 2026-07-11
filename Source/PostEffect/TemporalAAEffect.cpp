#include "pch.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraManager.h"
#include "PostEffect/PostEffectRenderTargets.h"
#include "Render/GBufferRenderTargets.h"
#include "TemporalAAEffect.h"

namespace
{
    float halton(uint32_t index, uint32_t base)
    {
        float f = 1.0f;
        float r = 0.0f;
        uint32_t i = index;

        while (i > 0)
        {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(i % base);
            i /= base;
        }

        return r;
    }
}

void TemporalAAEffect::initialize()
{
    m_psoKey = registerPSO(ShaderID::TemporalAAPS, RootSignatureType::PostEffectTemporal);
    m_cb = DXMem::makeUnique<ConstantBuffer<CBuffer>>();
    m_haltonIndex = 1;
    m_prevJitter = Vector2::Zero;
}

void TemporalAAEffect::render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex)
{
    auto* camera = CameraManager::Instance().getMainCamera();
    if (!cmd || !camera || inputSrvIndex == UINT_MAX)
    {
        return;
    }

    const UINT width = DX12::Instance().getScreenWidth();
    const UINT height = DX12::Instance().getScreenHeight();
    if (width == 0 || height == 0)
    {
        return;
    }

    ensureHistoryResources(width, height);

    const Matrix currentViewProj = camera->getView() * camera->getProjection();
    const Matrix invViewProj = currentViewProj.Invert();

    const Vector3 camPos = camera->getPosition();
    const Vector3 camForward = camera->getForward();

    bool cameraCut = false;
    if (m_hasPrevViewProj)
    {
        const float posDelta = (camPos - m_prevCamPos).Length();
        const float dotForward = std::clamp(camForward.Dot(m_prevCamForward), -1.0f, 1.0f);
        const float angleDeg = DirectX::XMConvertToDegrees(std::acos(dotForward));
        cameraCut = (posDelta > m_cameraCutPositionThreshold) || (angleDeg > m_cameraCutAngleThresholdDeg);
    }

    const Vector2 currJitter = nextHaltonJitter();
    const float jitterX = currJitter.x / static_cast<float>(width);
    const float jitterY = currJitter.y / static_cast<float>(height);

    CBuffer params{};
    params.currentViewProj = currentViewProj;
    params.prevViewProj = m_prevViewProj;
    params.invViewProj = invViewProj;

    const float historyValid = (m_hasHistory && !cameraCut) ? 1.0f : 0.0f;
    params.blendParams = Vector4(
        std::clamp(m_stationaryBlend, 0.0f, 0.995f),
        std::clamp(m_motionBlend, 0.0f, 0.95f),
        std::max(1.0f, m_motionScale),
        historyValid);

    params.texelParams = Vector4(
        1.0f / static_cast<float>(width),
        1.0f / static_cast<float>(height),
        jitterX,
        jitterY);

    params.prevJitter = Vector4(
        m_prevJitter.x / static_cast<float>(width),
        m_prevJitter.y / static_cast<float>(height),
        0.0f,
        0.0f);

    m_cb->update(params);

    applyPSO(cmd);

    cmd->SetGraphicsRootConstantBufferView(0, m_cb->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(inputSrvIndex));
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_historySrv[m_historyReadIndex]));
    cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(GBufferRenderTargets::Instance().getSrvIndex(3)));

    drawFullscreenTriangle(cmd);

    auto& rt = PostEffectRenderTargets::Instance();
    rt.transitionWriteToCopySource(cmd);
    transitionHistoryToCopyDest(cmd, m_historyWriteIndex);
    cmd->CopyResource(m_history[m_historyWriteIndex].Get(), rt.getCurrentWriteResource());
    transitionHistoryToSRV(cmd, m_historyWriteIndex);

    m_historyReadIndex = m_historyWriteIndex;
    m_historyWriteIndex = 1 - m_historyReadIndex;

    m_hasHistory = true;
    m_hasPrevViewProj = true;
    m_prevViewProj = currentViewProj;
    m_prevCamPos = camPos;
    m_prevCamForward = camForward;
    m_prevJitter = currJitter;
}

void TemporalAAEffect::inspectGUI()
{
    ImGui::SeparatorText("Temporal AA");
    ImGui::SliderFloat("Stationary Blend", &m_stationaryBlend, 0.50f, 0.98f);
    ImGui::SliderFloat("Motion Blend", &m_motionBlend, 0.00f, 0.50f);
    ImGui::SliderFloat("Motion Scale", &m_motionScale, 10.0f, 300.0f);
    ImGui::SliderFloat("Camera Cut Pos", &m_cameraCutPositionThreshold, 0.5f, 15.0f);
    ImGui::SliderFloat("Camera Cut Angle", &m_cameraCutAngleThresholdDeg, 5.0f, 80.0f);

    if (ImGui::Button("Reset TAA History"))
    {
        m_hasHistory = false;
        m_hasPrevViewProj = false;
    }
}

void TemporalAAEffect::ensureHistoryResources(UINT width, UINT height)
{
    if (m_width == width && m_height == height && m_history[0] && m_history[1])
    {
        return;
    }

    releaseHistoryResources();

    m_width = width;
    m_height = height;

    auto* device = DX12::Instance().getDevice();
    if (!device)
    {
        return;
    }

    const DXGI_FORMAT format = DX12::Instance().getBackBufferFormat();

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    for (int i = 0; i < 2; ++i)
    {
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(m_history[i].ReleaseAndGetAddressOf()));
        LOG_HR(hr, "TemporalAAEffect: failed to create history resource");

        if (m_historySrv[i] == UINT_MAX)
        {
            m_historySrv[i] = DescriptorHeapManager::Instance().allocateRange();
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        device->CreateShaderResourceView(
            m_history[i].Get(),
            &srvDesc,
            DescriptorHeapManager::Instance().getCPUHandle(m_historySrv[i]));
        DescriptorHeapManager::Instance().syncToVisible(m_historySrv[i]);

        m_historyState[i] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    m_historyReadIndex = 0;
    m_historyWriteIndex = 1;
    m_hasHistory = false;
    m_hasPrevViewProj = false;
    m_prevJitter = Vector2::Zero;
}

void TemporalAAEffect::releaseHistoryResources()
{
    for (auto& history : m_history)
    {
        history.Reset();
    }
}

void TemporalAAEffect::transitionHistoryToSRV(ID3D12GraphicsCommandList* cmd, int index)
{
    if (!cmd || index < 0 || index >= 2 || !m_history[index])
    {
        return;
    }

    if (m_historyState[index] == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        return;
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_history[index].Get(),
        m_historyState[index],
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
    m_historyState[index] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void TemporalAAEffect::transitionHistoryToCopyDest(ID3D12GraphicsCommandList* cmd, int index)
{
    if (!cmd || index < 0 || index >= 2 || !m_history[index])
    {
        return;
    }

    if (m_historyState[index] == D3D12_RESOURCE_STATE_COPY_DEST)
    {
        return;
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_history[index].Get(),
        m_historyState[index],
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->ResourceBarrier(1, &barrier);
    m_historyState[index] = D3D12_RESOURCE_STATE_COPY_DEST;
}

Vector2 TemporalAAEffect::nextHaltonJitter()
{
    constexpr uint32_t kSequenceLength = 16;
    const uint32_t idx = (m_haltonIndex % kSequenceLength) + 1;
    ++m_haltonIndex;

    const float hx = halton(idx, 2);
    const float hy = halton(idx, 3);

    return Vector2(hx - 0.5f, hy - 0.5f);
}
