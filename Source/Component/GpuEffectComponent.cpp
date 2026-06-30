#include "pch.h"
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
    m_totalTime += dt;
    m_emitAccumulator += m_emitterParams.emitRate * dt;

    m_lastEmitCount = static_cast<UINT>(m_emitAccumulator);
    if (m_lastEmitCount > 0)
    {
        m_emitAccumulator -= m_lastEmitCount;
    }

    m_randomSeed = makeRandomSeed(m_totalTime, dt);

    if (auto* t = gameObject() ? gameObject()->getComponent<TransformComponent>() : nullptr)
    {
        m_emitOrigin = t->getPosition();
    }

    simulateParticles(dt);

    if (!m_renderCB)
        return;

    m_renderParams.renderMode = static_cast<UINT>(m_renderSettings.mode);
    m_renderParams.flipbookRows = m_renderSettings.flipbookRows;
    m_renderParams.flipbookCols = m_renderSettings.flipbookCols;
    m_renderParams.flipbookFps = m_renderSettings.flipbookFps;
    m_renderCB->update(m_renderParams);
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
    if (m_renderSrvTableBase != UINT_MAX) DescriptorHeapManager::Instance().free(m_renderSrvTableBase, 2);

    m_particlesCpu.clear();
    m_particleBuffer.Reset();
    m_particleBufferMapped = nullptr;
    m_renderCB.reset();
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

    ImGui::Text("Texture: %s", wstringToString(m_texturePath).c_str());

    ImGui::Separator();
    ImGui::Text("Runtime");
    ImGui::Text("Alive Particles: %u", m_aliveParticleCount);
    ImGui::Text("Frame Emit Count: %u", m_lastEmitCount);
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
        return;

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    PSOCreator::Instance().setPSO(m_renderPSOKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());
    cmd->SetGraphicsRootConstantBufferView(1, m_renderCB->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_renderSrvTableBase));

    if (m_aliveParticleCount == 0)
        return;

    cmd->DrawInstanced(6, m_aliveParticleCount, 0, 0);
}

void GpuEffectComponent::renderForward(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
        return;

    render(cmd);
}

void GpuEffectComponent::simulateParticles(float dt)
{
    const auto& emitter = m_emitterParams;
    const auto& particleParams = m_particleParams;

    if (m_maxParticles == 0)
    {
        m_aliveParticleCount = 0;
        m_particlesCpu.clear();
        return;
    }

    std::vector<Particle> nextParticles;
    nextParticles.reserve(m_maxParticles);

    for (const auto& srcParticle : m_particlesCpu)
    {
        Particle updated = srcParticle;
        updated.age += dt;

        if (updated.age >= updated.lifetime)
        {
            continue;
        }

        updated.velocity += particleParams.gravity * dt;
        updated.velocity *= std::max(0.0f, 1.0f - particleParams.drag * dt);
        updated.position += updated.velocity * dt;

        float t = std::clamp(updated.age / std::max(updated.lifetime, 0.0001f), 0.0f, 1.0f);
        updated.size = std::lerp(particleParams.startSize, particleParams.endSize, t);
        updated.color = Vector4(
            std::lerp(particleParams.startColor.x, particleParams.endColor.x, t),
            std::lerp(particleParams.startColor.y, particleParams.endColor.y, t),
            std::lerp(particleParams.startColor.z, particleParams.endColor.z, t),
            std::lerp(particleParams.startColor.w, particleParams.endColor.w, t));
        updated.rotation += updated.rotationSpeed * dt;

        nextParticles.push_back(updated);
    }

    UINT seed = m_randomSeed;
    for (UINT i = 0; i < m_lastEmitCount && nextParticles.size() < m_maxParticles; ++i)
    {
        Particle newParticle{};
        newParticle.position = m_emitOrigin + randomSpherePoint(seed, emitter.emitRadius);

        Vector3 direction = randomDirection(seed);
        direction.Normalize();
        direction = Vector3::Lerp(Vector3::UnitY, direction, emitter.spread);
        float speedT = nextRandom01(seed);
        newParticle.velocity = direction * std::lerp(particleParams.minSpeed, particleParams.maxSpeed, speedT);
        newParticle.age = 0.0f;
        newParticle.lifetime = std::lerp(particleParams.minLifetime, particleParams.maxLifetime, nextRandom01(seed));
        newParticle.size = particleParams.startSize;
        newParticle.rotation = nextRandom01(seed) * XM_2PI;
        newParticle.rotationSpeed = (nextRandom01(seed) * 2.0f - 1.0f) * particleParams.startRotationSpeed;
        newParticle.stretch = particleParams.stretchFactor;
        newParticle.color = particleParams.startColor;

        nextParticles.push_back(newParticle);
    }

    m_particlesCpu.swap(nextParticles);
    m_aliveParticleCount = static_cast<UINT>(m_particlesCpu.size());
}

void GpuEffectComponent::syncParticleBuffer()
{
    if (!m_particleBufferMapped)
        return;

    if (m_aliveParticleCount == 0)
        return;

    memcpy(m_particleBufferMapped, m_particlesCpu.data(), sizeof(Particle) * m_aliveParticleCount);
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

    createParticleBuffer();
    updateDescriptorTables();
}

void GpuEffectComponent::initializeResources()
{
    m_renderCB = DXMem::makeUnique<ConstantBuffer<RenderParams>>(1);

    // テクスチャをロード (ディスクリプタテーブル未割当なので updateDescriptorTables は後で呼ぶ)
    if (!m_texturePath.empty())
    {
        m_texture = TextureManager::Instance().load(m_texturePath);
    }

    createParticleBuffer();
    createPipelines();
    m_renderSrvTableBase = DescriptorHeapManager::Instance().allocateRange(2);

    updateDescriptorTables();
}

void GpuEffectComponent::createParticleBuffer()
{
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

void GpuEffectComponent::createPipelines()
{
    // Render PSO
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