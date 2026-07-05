#include "pch.h"
#include "System/TimeManager.h"
#include "Camera/CameraManager.h"
#include "GpuEffectComponent.h"
#include "Graphics/LoadTexture.h"
#include "Component/TransformComponent.h"

namespace
{
    UINT makeRandomSeed(float totalTime, float deltaTime)
    {
        UINT timeAsUint = 0;
        memcpy(&timeAsUint, &totalTime, sizeof(UINT));
        UINT dtAsUint = 0;
        memcpy(&dtAsUint, &deltaTime, sizeof(UINT));
        UINT input = timeAsUint ^ dtAsUint;
        UINT state = input * 747796405u + 2891336453u;
        UINT word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return (word >> 22u) ^ word;
    }
}

void GpuEffectComponent::start()
{
    if (!m_initialized)
    {
        initializeResources();
        m_initialized = true;
    }

    IRenderComponent::start();
}

void GpuEffectComponent::update()
{
    const float dt = TimeManager::Instance().getDeltaTime();
    m_totalTime += dt;
    m_randomSeed = makeRandomSeed(m_totalTime, dt);

    if (auto* t = gameObject() ? gameObject()->getComponent<TransformComponent>() : nullptr)
    {
        m_emitOrigin = t->getPosition();
    }

    if (!m_renderCB || !m_simCB)
    {
        return;
    }

    m_renderParams.renderMode = static_cast<UINT>(m_renderSettings.mode);
    m_renderParams.flipbookRows = m_renderSettings.flipbookRows;
    m_renderParams.flipbookCols = m_renderSettings.flipbookCols;
    m_renderParams.flipbookFps = m_renderSettings.flipbookFps;
    m_renderCB->update(m_renderParams);

    refreshSimulationParams(dt);
}

void GpuEffectComponent::refreshSimulationParams(float deltaTime)
{
    if (!m_simCB)
    {
        return;
    }

    m_simParams.deltaTime = deltaTime;
    m_simParams.totalTime = m_totalTime;
    m_simParams.emitRate = m_emitterParams.emitRate;
    m_simParams.emitRadius = m_emitterParams.emitRadius;
    m_simParams.maxParticles = m_maxParticles;
    m_simParams.emitterType = static_cast<UINT>(m_emitterParams.type);
    m_simParams.randomSeed = m_randomSeed;
    m_simParams.resetAll = m_resetSimulation ? 1u : 0u;
    m_simParams.spread = m_emitterParams.spread;
    m_simParams.coneAngle = m_emitterParams.coneAngle;
    m_simParams.coneHeight = m_emitterParams.coneHeight;
    m_simParams.emitterSize = m_emitterParams.emitterSize;
    m_simParams.emitOrigin = m_emitOrigin;

    m_simParams.minLifetime = m_particleParams.minLifetime;
    m_simParams.maxLifetime = m_particleParams.maxLifetime;
    m_simParams.minSpeed = m_particleParams.minSpeed;
    m_simParams.maxSpeed = m_particleParams.maxSpeed;
    m_simParams.startSize = m_particleParams.startSize;
    m_simParams.endSize = m_particleParams.endSize;
    m_simParams.drag = m_particleParams.drag;
    m_simParams.startRotationSpeed = m_particleParams.startRotationSpeed;
    m_simParams.stretchFactor = m_particleParams.stretchFactor;
    m_simParams.noiseStrength = m_particleParams.noiseStrength;
    m_simParams.noiseFrequency = m_particleParams.noiseFrequency;
    m_simParams.gravity = m_particleParams.gravity;
    m_simParams.startColor = m_particleParams.startColor;
    m_simParams.endColor = m_particleParams.endColor;

    m_simCB->update(m_simParams);
}

void GpuEffectComponent::onEnable()
{
    if (!m_initialized)
    {
        initializeResources();
        m_initialized = true;
    }

    IRenderComponent::onEnable();
}

void GpuEffectComponent::onDisable()
{
    IRenderComponent::onDisable();
}

void GpuEffectComponent::onDestroy()
{
    IRenderComponent::onDestroy();

    for (UINT& idx : m_particleSrvIndices)
    {
        if (idx != UINT_MAX)
        {
            DescriptorHeapManager::Instance().free(idx);
            idx = UINT_MAX;
        }
    }

    for (UINT& idx : m_particleUavIndices)
    {
        if (idx != UINT_MAX)
        {
            DescriptorHeapManager::Instance().free(idx);
            idx = UINT_MAX;
        }
    }

    if (m_renderSrvTableBase != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_renderSrvTableBase, 2);
        m_renderSrvTableBase = UINT_MAX;
    }

    if (m_computeTableBase != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_computeTableBase, 3);
        m_computeTableBase = UINT_MAX;
    }

    if (m_drawArgsUavIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_drawArgsUavIndex);
        m_drawArgsUavIndex = UINT_MAX;
    }

    m_particleBuffers[0].Reset();
    m_particleBuffers[1].Reset();
    m_drawArgsBuffer.Reset();
    m_renderCB.reset();
    m_simCB.reset();
    m_computePSO.Reset();
    m_drawCommandSignature.Reset();
}

void GpuEffectComponent::inspectGUI()
{
    ImGui::Text("GPU Effect System");

    auto& emitter = m_emitterParams;
    auto& particle = m_particleParams;
    auto& render = m_renderSettings;

    int maxParticles = static_cast<int>(m_maxParticles);
    if (ImGui::DragInt("Max Particles", &maxParticles, 100, 1, 1000000))
    {
        setMaxParticles(static_cast<UINT>(maxParticles));
    }

    ImGui::DragFloat("Emit Rate", &emitter.emitRate, 1.0f, 0.0f, 100000.0f);
    ImGui::DragFloat("Spread", &emitter.spread, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Radius", &emitter.emitRadius, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Start Size", &particle.startSize, 0.01f, 0.01f, 50.0f);
    ImGui::DragFloat("End Size", &particle.endSize, 0.01f, 0.01f, 50.0f);
    ImGui::ColorEdit4("Start Color", &particle.startColor.x);
    ImGui::ColorEdit4("End Color", &particle.endColor.x);
    ImGui::DragFloat3("Gravity", &particle.gravity.x, 0.01f, -100.0f, 100.0f);
    ImGui::DragFloat("Drag", &particle.drag, 0.01f, 0.0f, 20.0f);

    static const char* kEmitterTypes[] = { "Sphere", "Box", "Cone", "Ring" };
    int emitterType = static_cast<int>(emitter.type);
    if (ImGui::Combo("Emitter Type", &emitterType, kEmitterTypes, IM_ARRAYSIZE(kEmitterTypes)))
    {
        emitter.type = static_cast<EmitterType>(std::clamp(emitterType, 0, kEmitterTypeCount - 1));
    }

    if (emitter.type == EmitterType::Box)
    {
        ImGui::DragFloat3("Emitter Size", &emitter.emitterSize.x, 0.01f, 0.0f, 100.0f);
    }
    else if (emitter.type == EmitterType::Cone)
    {
        ImGui::DragFloat("Cone Angle", &emitter.coneAngle, 0.1f, 0.1f, 89.0f);
        ImGui::DragFloat("Cone Height", &emitter.coneHeight, 0.01f, 0.0f, 100.0f);
    }

    ImGui::Separator();
    ImGui::Text("Motion");
    ImGui::DragFloat("Min Lifetime", &particle.minLifetime, 0.01f, 0.01f, 100.0f);
    ImGui::DragFloat("Max Lifetime", &particle.maxLifetime, 0.01f, 0.01f, 100.0f);
    if (particle.minLifetime > particle.maxLifetime)
    {
        std::swap(particle.minLifetime, particle.maxLifetime);
    }

    ImGui::DragFloat("Min Speed", &particle.minSpeed, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Max Speed", &particle.maxSpeed, 0.01f, 0.0f, 100.0f);
    if (particle.minSpeed > particle.maxSpeed)
    {
        std::swap(particle.minSpeed, particle.maxSpeed);
    }

    ImGui::DragFloat("Start Rotation Speed", &particle.startRotationSpeed, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Stretch Factor", &particle.stretchFactor, 0.001f, 0.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Noise");
    ImGui::DragFloat("Noise Strength", &particle.noiseStrength, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Noise Frequency", &particle.noiseFrequency, 0.01f, 0.0f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Render");
    static const char* kRenderModes[] = { "Billboard", "Stretched", "Horizontal", "Vertical" };
    int renderMode = static_cast<int>(render.mode);
    if (ImGui::Combo("Render Mode", &renderMode, kRenderModes, IM_ARRAYSIZE(kRenderModes)))
    {
        render.mode = static_cast<RenderMode>(std::clamp(renderMode, 0, kRenderModeCount - 1));
    }

    int flipbookRows = static_cast<int>(render.flipbookRows);
    int flipbookCols = static_cast<int>(render.flipbookCols);
    if (ImGui::DragInt("Flipbook Rows", &flipbookRows, 1.0f, 1, 128))
    {
        render.flipbookRows = static_cast<UINT>(std::max(1, flipbookRows));
    }
    if (ImGui::DragInt("Flipbook Cols", &flipbookCols, 1.0f, 1, 128))
    {
        render.flipbookCols = static_cast<UINT>(std::max(1, flipbookCols));
    }
    ImGui::DragFloat("Flipbook FPS", &render.flipbookFps, 0.1f, 0.0f, 240.0f);

    if (ImGui::Button("Select Texture"))
    {
        std::vector<std::wstring> selectedFiles;
        DialogResult result = Dialog::openFile(
            selectedFiles,
            L"Select Particle Texture",
            L"",
            false);

        if (result == DialogResult::OK && !selectedFiles.empty())
        {
            setTexture(selectedFiles[0]);
        }
    }

    ImGui::Text("Texture: %s", wstringToString(m_texturePath).c_str());

    ImGui::Separator();
    ImGui::Text("Runtime");
    ImGui::Text("Render Particle Count: GPU Indirect");
    ImGui::Text("Random Seed: %u", m_randomSeed);
}

void GpuEffectComponent::render()
{
    auto* cmd = DX12::Instance().getGraphicsCommandList();
    render(cmd);
}

void GpuEffectComponent::render(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
    {
        return;
    }

    runGpuSimulation(cmd);

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    PSOCreator::Instance().setPSO(m_renderPSOKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());
    cmd->SetGraphicsRootConstantBufferView(1, m_renderCB->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_renderSrvTableBase));

    if (m_drawCommandSignature && m_drawArgsBuffer)
    {
        cmd->ExecuteIndirect(m_drawCommandSignature.Get(), 1, m_drawArgsBuffer.Get(), 0, nullptr, 0);
    }
}

void GpuEffectComponent::renderForward(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
    {
        return;
    }

    render(cmd);
}

void GpuEffectComponent::runGpuSimulation(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_computePSO || !m_simCB)
    {
        return;
    }

    // update() より先に render() が呼ばれても reset 初期化が欠落しないよう、毎回明示的に反映する
    m_simParams.maxParticles = m_maxParticles;
    m_simParams.resetAll = m_resetSimulation ? 1u : 0u;
    m_simCB->update(m_simParams);

    const UINT readIdx = m_readBufferIndex;
    const UINT writeIdx = 1u - m_readBufferIndex;

    if (m_particleStates[readIdx] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    {
        auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_particleBuffers[readIdx].Get(),
            m_particleStates[readIdx],
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &toSrv);
        m_particleStates[readIdx] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    if (m_particleStates[writeIdx] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(
            m_particleBuffers[writeIdx].Get(),
            m_particleStates[writeIdx],
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &toUav);
        m_particleStates[writeIdx] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    if (m_drawArgsState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(
            m_drawArgsBuffer.Get(),
            m_drawArgsState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &toUav);
        m_drawArgsState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    updateDescriptorTables();

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    const UINT clearValues[4] = { 0, 0, 0, 0 };
    cmd->ClearUnorderedAccessViewUint(
        DescriptorHeapManager::Instance().getGPUHandle(m_drawArgsUavIndex),
        DescriptorHeapManager::Instance().getCPUHandle(m_drawArgsUavIndex),
        m_drawArgsBuffer.Get(),
        clearValues,
        0,
        nullptr);

    auto drawArgsClearUavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_drawArgsBuffer.Get());
    cmd->ResourceBarrier(1, &drawArgsClearUavBarrier);

    cmd->SetComputeRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::GpuEffectCompute));
    cmd->SetPipelineState(m_computePSO.Get());

    cmd->SetComputeRootConstantBufferView(0, m_simCB->getGPUAddress());
    cmd->SetComputeRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(m_computeTableBase));
    cmd->SetComputeRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_computeTableBase + 1));

    const UINT dispatchX = std::max(1u, (m_maxParticles + 255u) / 256u);
    cmd->Dispatch(dispatchX, 1, 1);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_particleBuffers[writeIdx].Get());
    cmd->ResourceBarrier(1, &uavBarrier);

    auto drawArgsUavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_drawArgsBuffer.Get());
    cmd->ResourceBarrier(1, &drawArgsUavBarrier);

    auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
        m_particleBuffers[writeIdx].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toSrv);
    m_particleStates[writeIdx] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    auto drawArgsToIndirect = CD3DX12_RESOURCE_BARRIER::Transition(
        m_drawArgsBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    cmd->ResourceBarrier(1, &drawArgsToIndirect);
    m_drawArgsState = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

    m_readBufferIndex = writeIdx;
    m_resetSimulation = false;

    updateDescriptorTables();
}

void GpuEffectComponent::setTexture(const std::wstring& path)
{
    if (path.empty())
    {
        return;
    }

    m_texturePath = path;
    m_texture = TextureManager::Instance().load(path);
    updateDescriptorTables();
}

void GpuEffectComponent::setMaxParticles(UINT maxParticles)
{
    if (maxParticles == 0)
    {
        maxParticles = 1;
    }

    if (maxParticles == m_maxParticles)
    {
        return;
    }

    m_maxParticles = maxParticles;
    m_aliveParticleCount = maxParticles;
    m_resetSimulation = true;

    createParticleBuffers();
    updateDescriptorTables();
}

void GpuEffectComponent::initializeResources()
{
    m_renderCB = DXMem::makeUnique<ConstantBuffer<RenderParams>>(1);
    m_simCB = DXMem::makeUnique<ConstantBuffer<SimParams>>(1);

    if (!m_texturePath.empty())
    {
        m_texture = TextureManager::Instance().load(m_texturePath);
    }

    // 初回描画が update() より先でも、シミュレーション定数が有効値になるよう事前にアップロード
    m_resetSimulation = true;
    refreshSimulationParams(0.0f);

    createParticleBuffers();
    createGpuDrivenDrawResources();
    createPipelines();

    if (m_renderSrvTableBase == UINT_MAX)
    {
        m_renderSrvTableBase = DescriptorHeapManager::Instance().allocateRange(2);
    }

    if (m_computeTableBase == UINT_MAX)
    {
        m_computeTableBase = DescriptorHeapManager::Instance().allocateRange(3);
    }

    m_aliveParticleCount = m_maxParticles;
    m_resetSimulation = true;
    updateDescriptorTables();
}

void GpuEffectComponent::createParticleBuffers()
{
    for (UINT& idx : m_particleSrvIndices)
    {
        if (idx != UINT_MAX)
        {
            DescriptorHeapManager::Instance().free(idx);
            idx = UINT_MAX;
        }
    }

    for (UINT& idx : m_particleUavIndices)
    {
        if (idx != UINT_MAX)
        {
            DescriptorHeapManager::Instance().free(idx);
            idx = UINT_MAX;
        }
    }

    m_particleBuffers[0].Reset();
    m_particleBuffers[1].Reset();

    auto* device = DX12::Instance().getDevice();
    const UINT64 bufferSize = std::max<UINT64>(1, sizeof(Particle) * m_maxParticles);

    for (UINT i = 0; i < 2; ++i)
    {
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(m_particleBuffers[i].GetAddressOf()));
        LOG_HR(hr, "GpuEffect: create particle default buffer failed");

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = m_maxParticles;
        srvDesc.Buffer.StructureByteStride = sizeof(Particle);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        m_particleSrvIndices[i] = DescriptorHeapManager::Instance().createSRV(m_particleBuffers[i].Get(), srvDesc);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = m_maxParticles;
        uavDesc.Buffer.StructureByteStride = sizeof(Particle);
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        m_particleUavIndices[i] = DescriptorHeapManager::Instance().createUAV(m_particleBuffers[i].Get(), nullptr, uavDesc);

        m_particleStates[i] = D3D12_RESOURCE_STATE_COMMON;
    }

    m_readBufferIndex = 0;
}

void GpuEffectComponent::createGpuDrivenDrawResources()
{
    if (m_drawArgsUavIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_drawArgsUavIndex);
        m_drawArgsUavIndex = UINT_MAX;
    }

    m_drawArgsBuffer.Reset();

    auto* device = DX12::Instance().getDevice();

    {
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_DRAW_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(m_drawArgsBuffer.ReleaseAndGetAddressOf()));
        LOG_HR(hr, "GpuEffect: create indirect draw-args buffer failed");
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = static_cast<UINT>(sizeof(D3D12_DRAW_ARGUMENTS) / sizeof(UINT));
    uavDesc.Buffer.StructureByteStride = 0;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    m_drawArgsUavIndex = DescriptorHeapManager::Instance().createUAV(m_drawArgsBuffer.Get(), nullptr, uavDesc);
    m_drawArgsState = D3D12_RESOURCE_STATE_COMMON;
}

void GpuEffectComponent::createPipelines()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::GpuEffectRender;
    psoData.vsShaderId = ShaderID::GpuEffectVS;
    psoData.psShaderId = ShaderID::GpuEffectPS;
    psoData.rasterizerState = RasterizerState::CULL_NONE;
    psoData.blendState = BlendState::ADD;
    psoData.depthStencilState = DepthStencilState::DEPTH_READ;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout = {};
    m_renderPSOKey = PSOCreator::Instance().registerPSO(psoData);

    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    computeDesc.pRootSignature = RootSignatureManager::Instance().getRootSignature(RootSignatureType::GpuEffectCompute);

    ID3DBlob* cs = ShaderManager::Instance().getShaderBlob(ShaderID::GpuEffectCS);
    if (cs)
    {
        computeDesc.CS.pShaderBytecode = cs->GetBufferPointer();
        computeDesc.CS.BytecodeLength = cs->GetBufferSize();
    }

    HRESULT hr = DX12::Instance().getDevice()->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(m_computePSO.ReleaseAndGetAddressOf()));
    LOG_HR(hr, "GpuEffect: create compute PSO failed");

    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;

    hr = DX12::Instance().getDevice()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(m_drawCommandSignature.ReleaseAndGetAddressOf()));
    LOG_HR(hr, "GpuEffect: create draw command signature failed");
}

void GpuEffectComponent::updateDescriptorTables()
{
    if (m_renderSrvTableBase != UINT_MAX && m_texture)
    {
        std::vector<UINT> renderSrvIndices =
        {
            m_particleSrvIndices[m_readBufferIndex],
            m_texture->getSRVIndex()
        };
        DescriptorHeapManager::Instance().copyDescriptorsRange(m_renderSrvTableBase, renderSrvIndices);
    }

    if (m_computeTableBase != UINT_MAX)
    {
        const UINT writeIndex = 1u - m_readBufferIndex;
        std::vector<UINT> computeIndices =
        {
            m_particleSrvIndices[m_readBufferIndex],
            m_particleUavIndices[writeIndex],
            m_drawArgsUavIndex
        };
        DescriptorHeapManager::Instance().copyDescriptorsRange(m_computeTableBase, computeIndices);
    }
}
