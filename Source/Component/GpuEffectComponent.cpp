#include "pch.h"
#include "GpuEffectComponent.h"
#include "Graphics/LoadTexture.h"
#include "Component/TransformComponent.h"

namespace
{
    float nextRandom01(UINT& state)
    {
        state = state * 747796405u + 2891336453u;
        UINT word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return static_cast<float>((word >> 22u) ^ word) / 4294967295.0f;
    }

    Vector3 randomDirection(UINT& state)
    {
        float angle = nextRandom01(state) * XM_2PI;
        float y = nextRandom01(state) * 2.0f - 1.0f;
        float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
        return Vector3(radius * std::cos(angle), y, radius * std::sin(angle));
    }

    Vector3 randomSpherePoint(UINT& state, float radius)
    {
        float scale = std::cbrt(nextRandom01(state));
        return randomDirection(state) * (radius * scale);
    }
}

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

    simulateParticles(dt);

    if (!m_simCB)
    {
        if (shouldOutputDebugLog())
            LOG_WARN("GpuEffect: SimCB is null (update skipped)");
        return;
    }

    m_simCB->update(m_simParams);
    syncParticleBuffer();
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

    if (m_particleSrvIndex != UINT_MAX) DescriptorHeapManager::Instance().free(m_particleSrvIndex);
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
    m_particlesCpu.clear();
    m_particleBuffer.Reset();
    m_particleBufferMapped = nullptr;
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
    ImGui::Text("GPU Effect System");

    int maxParticles = static_cast<int>(m_maxParticles);
    if (ImGui::DragInt("Max Particles", &maxParticles, 100, 1, 1000000))
    {
        setMaxParticles(static_cast<UINT>(maxParticles));
    }

    ImGui::DragFloat("Emit Rate", &m_emitRate, 1.0f, 0.0f, 100000.0f);
    ImGui::DragFloat("Lifetime", &m_lifetime, 0.01f, 0.01f, 100.0f);
    ImGui::DragFloat("Speed", &m_speed, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Spread", &m_spread, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Radius", &m_emitRadius, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Start Size", &m_startSize, 0.01f, 0.01f, 50.0f);
    ImGui::DragFloat("End Size", &m_endSize, 0.01f, 0.01f, 50.0f);
    ImGui::ColorEdit4("Start Color", &m_startColor.x);
    ImGui::ColorEdit4("End Color", &m_endColor.x);
    ImGui::DragFloat3("Gravity", &m_gravity.x, 0.01f, -100.0f, 100.0f);
    ImGui::DragFloat("Drag", &m_drag, 0.01f, 0.0f, 20.0f);

    ImGui::Text("Texture: %s", wstringToString(m_texturePath).c_str());

    ImGui::Separator();
    ImGui::Checkbox("Debug Log", &m_enableDebugLog);

    int interval = m_debugLogInterval;
    if (ImGui::DragInt("Log Interval (Frames)", &interval, 1.0f, 1, 600))
    {
        m_debugLogInterval = std::clamp(interval, 1, 600);
    }
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

    if (m_aliveParticleCount == 0)
    {
        return;
    }

    cmd->DrawInstanced(6, m_aliveParticleCount, 0, 0);

    if (shouldOutputDebugLog())
    {
        LOG_INFO("GpuEffect: draw executed (particles=%u, emit=%u)",
            m_aliveParticleCount,
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

    render(cmd);
}

void GpuEffectComponent::simulateParticles(float dt)
{
    if (m_maxParticles == 0)
    {
        m_aliveParticleCount = 0;
        m_particlesCpu.clear();
        return;
    }

    std::vector<Particle> nextParticles;
    nextParticles.reserve(m_maxParticles);

    for (const auto& particle : m_particlesCpu)
    {
        Particle updated = particle;
        updated.age += dt;

        if (updated.age >= updated.lifetime)
        {
            continue;
        }

        updated.velocity += m_gravity * dt;
        updated.velocity *= std::max(0.0f, 1.0f - m_drag * dt);
        updated.position += updated.velocity * dt;

        float t = std::clamp(updated.age / std::max(updated.lifetime, 0.0001f), 0.0f, 1.0f);
        updated.size = std::lerp(m_startSize, m_endSize, t);
        updated.color = Vector4(
            std::lerp(m_startColor.x, m_endColor.x, t),
            std::lerp(m_startColor.y, m_endColor.y, t),
            std::lerp(m_startColor.z, m_endColor.z, t),
            std::lerp(m_startColor.w, m_endColor.w, t));
        updated.rotation += updated.rotationSpeed * dt;

        nextParticles.push_back(updated);
    }

    UINT seed = m_simParams.randomSeed;
    for (UINT i = 0; i < m_simParams.emitCount && nextParticles.size() < m_maxParticles; ++i)
    {
        Particle particle{};
        particle.position = m_simParams.emitOrigin + randomSpherePoint(seed, m_emitRadius);

        Vector3 direction = randomDirection(seed);
        direction.Normalize();
        direction = Vector3::Lerp(Vector3::UnitY, direction, m_spread);
        float speedT = nextRandom01(seed);
        particle.velocity = direction * std::lerp(m_minSpeed, m_maxSpeed, speedT);
        particle.age = 0.0f;
        particle.lifetime = std::lerp(m_minLifetime, m_maxLifetime, nextRandom01(seed));
        particle.size = m_startSize;
        particle.rotation = nextRandom01(seed) * XM_2PI;
        particle.rotationSpeed = (nextRandom01(seed) * 2.0f - 1.0f) * m_startRotationSpeed;
        particle.stretch = 1.0f;
        particle.color = m_startColor;

        nextParticles.push_back(particle);
    }

    m_particlesCpu.swap(nextParticles);
    m_aliveParticleCount = static_cast<UINT>(m_particlesCpu.size());
}

void GpuEffectComponent::syncParticleBuffer()
{
    if (!m_particleBufferMapped)
    {
        return;
    }

    if (m_aliveParticleCount == 0)
    {
        return;
    }

    memcpy(m_particleBufferMapped, m_particlesCpu.data(), sizeof(Particle) * m_aliveParticleCount);
}

void GpuEffectComponent::simulate(ID3D12GraphicsCommandList* cmd)
{
    UNREFERENCED_PARAMETER(cmd);
    syncParticleBuffer();
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

    if (m_particlesCpu.size() > m_maxParticles)
    {
        m_particlesCpu.resize(m_maxParticles);
    }

    createParticleBuffer(0);
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
    createPipelines();
    m_renderSrvTableBase = DescriptorHeapManager::Instance().allocateRange(2);

    updateDescriptorTables();
}

void GpuEffectComponent::createParticleBuffer(int index)
{
    UNREFERENCED_PARAMETER(index);

    if (m_particleSrvIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_particleSrvIndex);
        m_particleSrvIndex = UINT_MAX;
    }

    m_particleBuffer.Reset();
    m_particleBufferMapped = nullptr;

    auto device = DX12::Instance().getDevice();
    UINT64 bufferSize = std::max<UINT64>(1, sizeof(Particle) * m_maxParticles);
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_particleBuffer.GetAddressOf()));
    LOG_HR(hr, "GpuEffect: create particle upload buffer failed");

    if (m_particleBuffer)
    {
        void* mapped = nullptr;
        hr = m_particleBuffer->Map(0, nullptr, &mapped);
        LOG_HR(hr, "GpuEffect: map particle upload buffer failed");
        m_particleBufferMapped = reinterpret_cast<Particle*>(mapped);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = m_maxParticles;
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    m_particleSrvIndex = DescriptorHeapManager::Instance().createSRV(m_particleBuffer.Get(), srvDesc);
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
}

void GpuEffectComponent::updateDescriptorTables()
{
    if (m_renderSrvTableBase != UINT_MAX && m_texture && m_particleSrvIndex != UINT_MAX)
    {
        std::vector<UINT> srvIndices =
        {
            m_particleSrvIndex,
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