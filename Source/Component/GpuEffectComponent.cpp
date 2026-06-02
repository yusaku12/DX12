#include "pch.h"
#include "GpuEffectComponent.h"
#include "Graphics/LoadTexture.h"
#include "Component/TransformComponent.h"

void GpuEffectComponent::awake()
{
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
    float dt = TimeManager::Instance().getDeltaTime();
    m_emitAccumulator += m_emitRate * dt;

    UINT emitCount = static_cast<UINT>(m_emitAccumulator);
    if (emitCount > 0)
    {
        m_emitAccumulator -= emitCount;
    }

    m_simParams.deltaTime = dt;
    m_simParams.totalTime += dt;
    m_simParams.emitRate = m_emitRate;
    m_simParams.emitCount = emitCount;
    m_simParams.maxParticles = m_maxParticles;
    m_simParams.lifetime = m_lifetime;
    m_simParams.speed = m_speed;
    m_simParams.spread = m_spread;
    m_simParams.startSize = m_startSize;
    m_simParams.endSize = m_endSize;
    m_simParams.drag = m_drag;
    m_simParams.emitterType = m_emitterType;
    m_simParams.emitRadius = m_emitRadius;
    m_simParams.startColor = m_startColor;
    m_simParams.endColor = m_endColor;

    m_simParams.gravity = m_gravity;
    m_simParams.noiseStrength = m_noiseStrength;
    m_simParams.emitterSize = m_emitterSize;
    m_simParams.noiseFrequency = m_noiseFrequency;
    m_simParams.coneAngle = m_coneAngle;
    m_simParams.coneHeight = m_coneHeight;
    m_simParams.minLifetime = m_minLifetime;
    m_simParams.maxLifetime = m_maxLifetime;
    m_simParams.minSpeed = m_minSpeed;
    m_simParams.maxSpeed = m_maxSpeed;
    m_simParams.startRotationSpeed = m_startRotationSpeed;
    m_simParams.stretchFactor = m_stretchFactor;

    m_simParams.renderMode = m_renderMode;
    m_simParams.flipbookRows = m_flipbookRows;
    m_simParams.flipbookCols = m_flipbookCols;
    m_simParams.flipbookFps = m_flipbookFps;

    // CPU側でフレームごとのランダムシードを事前計算 (GPU側の冗長なALU演算を削減)
    {
        UINT timeAsUint = 0;
        float totalTime = m_simParams.totalTime;
        memcpy(&timeAsUint, &totalTime, sizeof(UINT));
        UINT dtAsUint = 0;
        memcpy(&dtAsUint, &dt, sizeof(UINT));
        UINT input = timeAsUint ^ dtAsUint;
        UINT state = input * 747796405u + 2891336453u;
        UINT word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        m_simParams.randomSeed = (word >> 22u) ^ word;
    }

    if (auto* t = gameObject() ? gameObject()->getComponent<TransformComponent>() : nullptr)
    {
        m_simParams.emitOrigin = t->getPosition();
    }

    if (!m_simCB)
    {
        if (shouldOutputDebugLog())
            LOG_WARN("GpuEffect: SimCB is null (update skipped)");
        return;
    }

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

    for (auto& p : m_particles)
    {
        if (p.srvIndex != UINT_MAX) DescriptorHeapManager::Instance().free(p.srvIndex);
        if (p.uavIndex != UINT_MAX) DescriptorHeapManager::Instance().free(p.uavIndex);
    }

    if (m_aliveCountSrvIndex != UINT_MAX) DescriptorHeapManager::Instance().free(m_aliveCountSrvIndex);
    if (m_computeUavTableBase != UINT_MAX) DescriptorHeapManager::Instance().free(m_computeUavTableBase, 2);
    if (m_renderSrvTableBase != UINT_MAX) DescriptorHeapManager::Instance().free(m_renderSrvTableBase, 2);

    m_particles[0] = {};
    m_particles[1] = {};
    m_aliveCountBuffer.Reset();
    m_drawArgsBuffer.Reset();
    m_drawArgsUpload.Reset();
    m_drawCommandSignature.Reset();
    m_counterResetUpload.Reset();
    m_computePSO.Reset();
    m_simCB.reset();

    if (m_enableDebugLog)
        LOG_INFO("GpuEffect: destroyed and resources released");
}

void GpuEffectComponent::inspectGUI()
{
    ImGui::Text("GPU Effect System (Ultra Settings)");
    
    int maxParticles = static_cast<int>(m_maxParticles);
    if (ImGui::DragInt("Max Particles", &maxParticles, 100, 1, 1000000))
    {
        setMaxParticles(static_cast<UINT>(maxParticles));
    }

    if (ImGui::CollapsingHeader("Emission Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Emit Rate", &m_emitRate, 1.0f, 0.0f, 100000.0f);
        ImGui::DragFloat("Min Lifetime", &m_minLifetime, 0.01f, 0.01f, 100.0f);
        ImGui::DragFloat("Max Lifetime", &m_maxLifetime, 0.01f, 0.01f, 100.0f);
        if (m_maxLifetime < m_minLifetime) m_maxLifetime = m_minLifetime;
        m_lifetime = m_maxLifetime; // 互換性維持

        ImGui::DragFloat("Min Speed", &m_minSpeed, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("Max Speed", &m_maxSpeed, 0.01f, 0.0f, 100.0f);
        if (m_maxSpeed < m_minSpeed) m_maxSpeed = m_minSpeed;
        m_speed = m_maxSpeed; // 互換性維持

        ImGui::DragFloat("Spread (Angle)", &m_spread, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Emitter Shape", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* shapes[] = { "Sphere", "Box", "Cone", "Ring" };
        int currentShape = static_cast<int>(m_emitterType);
        if (ImGui::Combo("Type", &currentShape, shapes, IM_ARRAYSIZE(shapes)))
        {
            m_emitterType = static_cast<UINT>(currentShape);
        }

        if (m_emitterType == 0 || m_emitterType == 2 || m_emitterType == 3) // Sphere, Cone, Ring
        {
            ImGui::DragFloat("Radius", &m_emitRadius, 0.01f, 0.001f, 100.0f);
        }
        if (m_emitterType == 1) // Box
        {
            ImGui::DragFloat3("Box Size", &m_emitterSize.x, 0.01f, 0.001f, 100.0f);
        }
        if (m_emitterType == 2) // Cone
        {
            ImGui::DragFloat("Cone Angle", &m_coneAngle, 0.1f, 0.0f, 90.0f);
            ImGui::DragFloat("Cone Height", &m_coneHeight, 0.01f, 0.01f, 100.0f);
        }
    }

    if (ImGui::CollapsingHeader("Particle Over Lifetime (Size/Color/Rotation)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Start Size", &m_startSize, 0.01f, 0.01f, 50.0f);
        ImGui::DragFloat("End Size", &m_endSize, 0.01f, 0.01f, 50.0f);
        
        ImGui::ColorEdit4("Start Color", &m_startColor.x);
        ImGui::ColorEdit4("End Color", &m_endColor.x);

        ImGui::DragFloat("Rotation Speed", &m_startRotationSpeed, 0.01f, -100.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("Forces & Physics"))
    {
        ImGui::DragFloat3("Gravity Vector", &m_gravity.x, 0.01f, -100.0f, 100.0f);
        ImGui::DragFloat("Air Resistance (Drag)", &m_drag, 0.01f, 0.0f, 20.0f);
        
        ImGui::Separator();
        ImGui::Text("Turbulence / Noise");
        ImGui::DragFloat("Noise Strength", &m_noiseStrength, 0.01f, 0.0f, 50.0f);
        ImGui::DragFloat("Noise Frequency", &m_noiseFrequency, 0.01f, 0.001f, 10.0f);
    }

    if (ImGui::CollapsingHeader("Rendering Modes & Texture", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* renderModes[] = { "Billboard", "Stretched Billboard", "Horizontal Flat", "Vertical Flat" };
        int mode = static_cast<int>(m_renderMode);
        if (ImGui::Combo("Render Mode", &mode, renderModes, IM_ARRAYSIZE(renderModes)))
        {
            m_renderMode = static_cast<UINT>(mode);
        }

        if (m_renderMode == 1) // Stretched
        {
            ImGui::DragFloat("Stretch Factor", &m_stretchFactor, 0.01f, 0.0f, 10.0f);
        }

        ImGui::Text("Texture: %s", wstringToString(m_texturePath).c_str());

        // Texture sprite/flipbook animation settings
        ImGui::Separator();
        ImGui::Text("Flipbook (Sprite Sheet) Animation");
        int rows = static_cast<int>(m_flipbookRows);
        int cols = static_cast<int>(m_flipbookCols);
        if (ImGui::DragInt("Flipbook Rows", &rows, 1.0f, 1, 64)) m_flipbookRows = static_cast<UINT>(rows);
        if (ImGui::DragInt("Flipbook Cols", &cols, 1.0f, 1, 64)) m_flipbookCols = static_cast<UINT>(cols);
        ImGui::DragFloat("Flipbook FPS (0 = Sync to Life)", &m_flipbookFps, 0.1f, 0.0f, 120.0f);
    }

    ImGui::Separator();
    ImGui::Checkbox("Debug Log", &m_enableDebugLog);

    int interval = m_debugLogInterval;
    if (ImGui::DragInt("Log Interval (Frames)", &interval, 1.0f, 1, 600))
    {
        m_debugLogInterval = std::clamp(interval, 1, 600);
    }

    ImGui::Checkbox("Force Draw (No ExecuteIndirect)", &m_debugForceDraw);
    ImGui::Checkbox("Force DrawArgs InstanceCount=1", &m_debugForceIndirectArgs);
}

void GpuEffectComponent::render()
{
    auto* cmd = DX12::Instance().getGraphicsCommandList();
    renderForward(cmd);
}

void GpuEffectComponent::render(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
    {
        if (shouldOutputDebugLog())
            LOG_WARN("GpuEffect: render skipped (cmd=%p, initialized=%d)", cmd, m_initialized);
        return;
    }

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    PSOCreator::Instance().setPSO(m_renderPSOKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());
    cmd->SetGraphicsRootConstantBufferView(1, m_simCB->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_renderSrvTableBase));

    if (m_debugForceDraw)
    {
        cmd->DrawInstanced(6, 1, 0, 0);
        if (shouldOutputDebugLog())
            LOG_INFO("GpuEffect: ForceDraw executed");
        return;
    }

    if (m_debugForceIndirectArgs && m_drawArgsUpload)
    {
        D3D12_DRAW_ARGUMENTS args = { 6, 1, 0, 0 };

        void* mapped = nullptr;
        m_drawArgsUpload->Map(0, nullptr, &mapped);
        memcpy(mapped, &args, sizeof(args));
        m_drawArgsUpload->Unmap(0, nullptr);

        auto transition = [&](D3D12_RESOURCE_STATES newState)
            {
                if (m_drawArgsState == newState) return;
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_drawArgsBuffer.Get(), m_drawArgsState, newState);
                cmd->ResourceBarrier(1, &barrier);
                m_drawArgsState = newState;
            };

        transition(D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyBufferRegion(m_drawArgsBuffer.Get(), 0, m_drawArgsUpload.Get(), 0, sizeof(args));
        transition(D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    }

    if (!m_drawCommandSignature)
    {
        if (shouldOutputDebugLog())
            LOG_WARN("GpuEffect: draw command signature is null");
        return;
    }

    cmd->ExecuteIndirect(m_drawCommandSignature.Get(), 1, m_drawArgsBuffer.Get(), 0, nullptr, 0);

    if (shouldOutputDebugLog())
    {
        LOG_INFO("GpuEffect: draw executed (srv=%u, uav=%u, emit=%u)",
            m_particles[m_currentIndex].srvIndex,
            m_particles[m_currentIndex].uavIndex,
            m_simParams.emitCount);
    }
}

void GpuEffectComponent::renderForward(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
    {
        if (shouldOutputDebugLog())
            LOG_WARN("GpuEffect: renderForward skipped (cmd=%p, initialized=%d)", cmd, m_initialized);
        return;
    }

    simulate(cmd);
    render(cmd);
}

void GpuEffectComponent::simulate(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized) return;

    int inIndex = m_currentIndex;
    int outIndex = 1 - m_currentIndex;

    auto transition = [&](ID3D12Resource* res, D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES newState)
        {
            if (!res || state == newState) return;
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(res, state, newState);
            cmd->ResourceBarrier(1, &barrier);
            state = newState;
        };

    if (!m_countersInitialized && m_counterResetUpload)
    {
        for (int i = 0; i < 2; ++i)
        {
            transition(m_particles[i].counter.Get(), m_counterStates[i], D3D12_RESOURCE_STATE_COPY_DEST);
            cmd->CopyBufferRegion(m_particles[i].counter.Get(), 0, m_counterResetUpload.Get(), 0, sizeof(UINT));
        }

        transition(m_aliveCountBuffer.Get(), m_aliveCountState, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyBufferRegion(m_aliveCountBuffer.Get(), 0, m_counterResetUpload.Get(), 0, sizeof(UINT));

        // DrawArgs の静的部分を一度だけ初期化 (VertexCount=6, StartVertex=0, StartInstance=0)
        if (m_drawArgsUpload)
        {
            transition(m_drawArgsBuffer.Get(), m_drawArgsState, D3D12_RESOURCE_STATE_COPY_DEST);
            cmd->CopyBufferRegion(m_drawArgsBuffer.Get(), 0, m_drawArgsUpload.Get(), 0, sizeof(D3D12_DRAW_ARGUMENTS));
        }

        m_countersInitialized = true;
    }

    // コピー前の状態遷移
    transition(m_particles[outIndex].counter.Get(), m_counterStates[outIndex], D3D12_RESOURCE_STATE_COPY_DEST);
    transition(m_particles[inIndex].counter.Get(), m_counterStates[inIndex], D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(m_aliveCountBuffer.Get(), m_aliveCountState, D3D12_RESOURCE_STATE_COPY_DEST);

    // カウンタリセット
    cmd->CopyBufferRegion(m_particles[outIndex].counter.Get(), 0, m_counterResetUpload.Get(), 0, sizeof(UINT));

    // aliveCount を取得
    cmd->CopyBufferRegion(m_aliveCountBuffer.Get(), 0, m_particles[inIndex].counter.Get(), 0, sizeof(UINT));

    // aliveCount を SRV 化
    transition(m_aliveCountBuffer.Get(), m_aliveCountState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // バッファを UAV 化
    transition(m_particles[inIndex].buffer.Get(), m_particleStates[inIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_particles[outIndex].buffer.Get(), m_particleStates[outIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_particles[inIndex].counter.Get(), m_counterStates[inIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_particles[outIndex].counter.Get(), m_counterStates[outIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetComputeRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::GpuEffectCompute));
    cmd->SetPipelineState(m_computePSO.Get());

    cmd->SetComputeRootConstantBufferView(0, m_simCB->getGPUAddress());
    cmd->SetComputeRootDescriptorTable(1, DescriptorHeapManager::Instance().getGPUHandle(m_aliveCountSrvIndex));

    updateDescriptorTables();
    cmd->SetComputeRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_computeUavTableBase));

    constexpr UINT THREADS = 256;
    UINT groups = (m_maxParticles + THREADS - 1) / THREADS;
    cmd->Dispatch(groups, 1, 1);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_particles[outIndex].buffer.Get());
    cmd->ResourceBarrier(1, &uavBarrier);

    auto counterUavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_particles[outIndex].counter.Get());
    cmd->ResourceBarrier(1, &counterUavBarrier);

    // DrawArgs 更新: InstanceCount のみ毎フレーム更新
    // 静的部分 (VertexCount=6, StartVertex=0, StartInstance=0) は初期化時に設定済み
    transition(m_drawArgsBuffer.Get(), m_drawArgsState, D3D12_RESOURCE_STATE_COPY_DEST);
    transition(m_particles[outIndex].counter.Get(), m_counterStates[outIndex], D3D12_RESOURCE_STATE_COPY_SOURCE);
    cmd->CopyBufferRegion(m_drawArgsBuffer.Get(), sizeof(UINT), m_particles[outIndex].counter.Get(), 0, sizeof(UINT));

    transition(m_drawArgsBuffer.Get(), m_drawArgsState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    // 描画用に SRV 化
    transition(m_particles[outIndex].buffer.Get(), m_particleStates[outIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    m_currentIndex = outIndex;
    updateDescriptorTables();
}

void GpuEffectComponent::setTexture(const std::wstring& path)
{
    if (path.empty()) return;
    m_texturePath = path;
    m_texture = TextureManager::Instance().load(path);
    updateDescriptorTables();
}

void GpuEffectComponent::setMaxParticles(UINT maxParticles)
{
    if (maxParticles == 0) maxParticles = 1;
    if (maxParticles == m_maxParticles) return;

    m_maxParticles = maxParticles;

    for (auto& p : m_particles)
    {
        if (p.srvIndex != UINT_MAX) DescriptorHeapManager::Instance().free(p.srvIndex);
        if (p.uavIndex != UINT_MAX) DescriptorHeapManager::Instance().free(p.uavIndex);
    }

    m_particles[0] = {};
    m_particles[1] = {};
    createParticleBuffer(0);
    createParticleBuffer(1);
    updateDescriptorTables();
}

void GpuEffectComponent::initializeResources()
{
    m_simCB = std::make_unique<ConstantBuffer<SimParams>>(1);

    // テクスチャをロード (ディスクリプタテーブル未割当なので updateDescriptorTables は後で呼ぶ)
    if (!m_texturePath.empty())
    {
        m_texture = TextureManager::Instance().load(m_texturePath);
    }

    createParticleBuffer(0);
    createParticleBuffer(1);
    createAliveCountBuffer();
    createDrawArgsBuffer();
    createPipelines();

    m_computeUavTableBase = DescriptorHeapManager::Instance().allocateRange(2);
    m_renderSrvTableBase = DescriptorHeapManager::Instance().allocateRange(2);

    updateDescriptorTables();
}

void GpuEffectComponent::createParticleBuffer(int index)
{
    auto device = DX12::Instance().getDevice();
    UINT64 bufferSize = sizeof(Particle) * m_maxParticles;

    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_particles[index].buffer.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create particle buffer failed");

    auto counterDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &counterDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_particles[index].counter.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create counter buffer failed");

    m_particleStates[index] = D3D12_RESOURCE_STATE_COMMON;
    m_counterStates[index] = D3D12_RESOURCE_STATE_COMMON;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = m_maxParticles;
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.NumElements = m_maxParticles;
    uavDesc.Buffer.StructureByteStride = sizeof(Particle);

    m_particles[index].srvIndex = DescriptorHeapManager::Instance().createSRV(m_particles[index].buffer.Get(), srvDesc);
    m_particles[index].uavIndex = DescriptorHeapManager::Instance().createUAV(m_particles[index].buffer.Get(), m_particles[index].counter.Get(), uavDesc);

    if (!m_counterResetUpload)
    {
        UINT zero = 0;
        auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT));
        CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        HRESULT hr2 = device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_counterResetUpload.GetAddressOf()));
        LOG_HR(hr2, "GpuEffect: create counter reset upload failed");

        void* mapped = nullptr;
        m_counterResetUpload->Map(0, nullptr, &mapped);
        memcpy(mapped, &zero, sizeof(UINT));
        m_counterResetUpload->Unmap(0, nullptr);
    }
}

void GpuEffectComponent::createDrawArgsBuffer()
{
    auto device = DX12::Instance().getDevice();

    D3D12_DRAW_ARGUMENTS args = { 6, 0, 0, 0 };

    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_DRAW_ARGUMENTS));
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_drawArgsBuffer.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create draw args buffer failed");

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_drawArgsUpload.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create draw args upload failed");

    void* mapped = nullptr;
    m_drawArgsUpload->Map(0, nullptr, &mapped);
    memcpy(mapped, &args, sizeof(args));
    m_drawArgsUpload->Unmap(0, nullptr);

    m_drawArgsState = D3D12_RESOURCE_STATE_COMMON;
}

void GpuEffectComponent::createAliveCountBuffer()
{
    auto device = DX12::Instance().getDevice();
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT));
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_aliveCountBuffer.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create alive count buffer failed");

    m_aliveCountState = D3D12_RESOURCE_STATE_COMMON;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = 1;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

    m_aliveCountSrvIndex = DescriptorHeapManager::Instance().createSRV(m_aliveCountBuffer.Get(), srvDesc);
}

void GpuEffectComponent::createPipelines()
{
    // Render PSO
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::GpuEffectRender;
    psoData.vsShaderId = ShaderID::GpuEffectVS;
    psoData.psShaderId = ShaderID::GpuEffectPS;
    psoData.rasterizerState = RasterizerState::CULL_NONE;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_READ;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout = {};
    m_renderPSOKey = PSOCreator::Instance().registerPSO(psoData);

    // Compute PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = RootSignatureManager::Instance().getRootSignature(RootSignatureType::GpuEffectCompute);
    auto* blob = ShaderManager::Instance().getShaderBlob(ShaderID::GpuEffectCS);
    desc.CS.pShaderBytecode = blob->GetBufferPointer();
    desc.CS.BytecodeLength = blob->GetBufferSize();

    HRESULT hr = DX12::Instance().getDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_computePSO.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create compute PSO failed");

    if (!m_drawCommandSignature)
    {
        D3D12_INDIRECT_ARGUMENT_DESC arg{};
        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

        D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
        sigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
        sigDesc.NumArgumentDescs = 1;
        sigDesc.pArgumentDescs = &arg;

        hr = DX12::Instance().getDevice()->CreateCommandSignature(
            &sigDesc, nullptr, IID_PPV_ARGS(m_drawCommandSignature.GetAddressOf()));
        LOG_HR(hr, "GpuEffect: create command signature failed");
    }
}

void GpuEffectComponent::updateDescriptorTables()
{
    if (m_computeUavTableBase != UINT_MAX)
    {
        std::vector<UINT> uavIndices =
        {
            m_particles[m_currentIndex].uavIndex,
            m_particles[1 - m_currentIndex].uavIndex
        };
        DescriptorHeapManager::Instance().copyDescriptorsRange(m_computeUavTableBase, uavIndices);
    }

    if (m_renderSrvTableBase != UINT_MAX && m_texture)
    {
        std::vector<UINT> srvIndices =
        {
            m_particles[m_currentIndex].srvIndex,
            m_texture->getSRVIndex()
        };
        DescriptorHeapManager::Instance().copyDescriptorsRange(m_renderSrvTableBase, srvIndices);
    }
}

bool GpuEffectComponent::shouldOutputDebugLog()
{
    if (!m_enableDebugLog) return false;
    ++m_debugFrameCounter;
    return (m_debugFrameCounter % m_debugLogInterval) == 0;
}