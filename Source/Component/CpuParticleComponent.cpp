#include "pch.h"
#include "System/TimeManager.h"
#include "Camera/CameraManager.h"
#include "CpuParticleComponent.h"

#include "Component/TransformComponent.h"
#include "Component/FbxRenderComponent.h"
#include "GameObject/GameObjectRegistry.h"
#include "Graphics/LoadTexture.h"
#include "Physics/PhysicsWorld.h"

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
        const float angle = nextRandom01(state) * XM_2PI;
        const float y = nextRandom01(state) * 2.0f - 1.0f;
        const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
        return Vector3(radius * std::cos(angle), y, radius * std::sin(angle));
    }

    Vector3 randomSpherePoint(UINT& state, float radius)
    {
        const float scale = std::cbrt(nextRandom01(state));
        return randomDirection(state) * (radius * scale);
    }

    Vector3 randomDirectionAround(UINT& state, const Vector3& axis, float spread)
    {
        Vector3 base = axis;
        if (base.LengthSquared() <= 1e-6f)
        {
            base = Vector3::UnitY;
        }
        base.Normalize();

        Vector3 randDir = randomDirection(state);
        randDir.Normalize();

        Vector3 blended = Vector3::Lerp(base, randDir, std::clamp(spread, 0.0f, 1.0f));
        if (blended.LengthSquared() <= 1e-6f)
        {
            return base;
        }

        blended.Normalize();
        return blended;
    }

    float safeInv(float v)
    {
        return (std::abs(v) <= 1e-6f) ? 0.0f : (1.0f / v);
    }
}

void CpuParticleComponent::awake()
{
    if (m_meshSourceObjectName.empty())
    {
        m_meshSourceObjectName = gameObject() ? gameObject()->getName() : std::string{};
    }
}

void CpuParticleComponent::start()
{
    if (!m_initialized)
    {
        initializeResources();
        m_initialized = true;
    }

    IRenderComponent::start();
}

void CpuParticleComponent::update()
{
    const float dt = TimeManager::Instance().getDeltaTime();
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
    {
        return;
    }

    m_renderParams.renderMode = static_cast<UINT>(m_renderSettings.mode);
    m_renderParams.flipbookRows = std::max(1u, m_renderSettings.flipbookRows);
    m_renderParams.flipbookCols = std::max(1u, m_renderSettings.flipbookCols);
    m_renderParams.flipbookFps = std::max(0.0f, m_renderSettings.flipbookFps);
    m_renderCB->update(m_renderParams);

    syncParticleBuffer();
}

void CpuParticleComponent::onEnable()
{
    if (!m_initialized)
    {
        initializeResources();
        m_initialized = true;
    }

    IRenderComponent::onEnable();
}

void CpuParticleComponent::onDisable()
{
    IRenderComponent::onDisable();
}

void CpuParticleComponent::onDestroy()
{
    IRenderComponent::onDestroy();

    if (m_particleSrvIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_particleSrvIndex);
        m_particleSrvIndex = UINT_MAX;
    }

    if (m_renderSrvTableBase != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_renderSrvTableBase, 2);
        m_renderSrvTableBase = UINT_MAX;
    }

    m_particlesCpu.clear();
    m_particleBuffer.Reset();
    m_particleBufferMapped = nullptr;
    m_renderCB.reset();
}

void CpuParticleComponent::inspectGUI()
{
    ImGui::Text("CPU Particle System");

    int maxParticles = static_cast<int>(m_maxParticles);
    if (ImGui::DragInt("Max Particles", &maxParticles, 100, 1, 1000000))
    {
        setMaxParticles(static_cast<UINT>(std::max(1, maxParticles)));
    }

    static const char* kEmitterTypes[] = { "Point", "Sphere", "Mesh Surface" };
    int emitterType = static_cast<int>(m_emitterParams.type);
    if (ImGui::Combo("Emitter Type", &emitterType, kEmitterTypes, IM_ARRAYSIZE(kEmitterTypes)))
    {
        m_emitterParams.type = static_cast<EmitterType>(std::clamp(emitterType, 0, kEmitterTypeCount - 1));
    }

    ImGui::DragFloat("Emit Rate", &m_emitterParams.emitRate, 1.0f, 0.0f, 100000.0f);
    ImGui::DragFloat("Spread", &m_emitterParams.spread, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat3("Emit Direction", &m_emitterParams.emitDirection.x, 0.01f, -1.0f, 1.0f);

    if (m_emitterParams.type == EmitterType::Sphere)
    {
        ImGui::DragFloat("Emit Radius", &m_emitterParams.emitRadius, 0.01f, 0.0f, 100.0f);
    }

    if (m_emitterParams.type == EmitterType::MeshSurface)
    {
        char meshSourceName[256]{};
        const std::string& srcName = m_meshSourceObjectName;
        const size_t copyLen = std::min(srcName.size(), sizeof(meshSourceName) - 1);
        memcpy(meshSourceName, srcName.c_str(), copyLen);
        if (ImGui::InputText("Mesh Source Object", meshSourceName, sizeof(meshSourceName)))
        {
            m_meshSourceObjectName = meshSourceName;
            m_meshTriangles.clear();
            m_meshTotalArea = 0.0f;
            m_meshCacheAge = m_emitterParams.meshCacheUpdateInterval;
        }

        ImGui::DragFloat("Mesh Cache Interval", &m_emitterParams.meshCacheUpdateInterval, 0.01f, 0.01f, 5.0f);
        ImGui::Text("Mesh Triangles: %zu", m_meshTriangles.size());
    }

    ImGui::Separator();
    ImGui::Text("Particle");
    ImGui::DragFloat("Start Size", &m_particleParams.startSize, 0.01f, 0.001f, 100.0f);
    ImGui::DragFloat("End Size", &m_particleParams.endSize, 0.01f, 0.001f, 100.0f);
    ImGui::ColorEdit4("Start Color", &m_particleParams.startColor.x);
    ImGui::ColorEdit4("End Color", &m_particleParams.endColor.x);
    ImGui::DragFloat3("Gravity", &m_particleParams.gravity.x, 0.01f, -100.0f, 100.0f);
    ImGui::DragFloat("Drag", &m_particleParams.drag, 0.01f, 0.0f, 20.0f);

    ImGui::DragFloat("Min Lifetime", &m_particleParams.minLifetime, 0.01f, 0.01f, 100.0f);
    ImGui::DragFloat("Max Lifetime", &m_particleParams.maxLifetime, 0.01f, 0.01f, 100.0f);
    if (m_particleParams.minLifetime > m_particleParams.maxLifetime)
    {
        std::swap(m_particleParams.minLifetime, m_particleParams.maxLifetime);
    }

    ImGui::DragFloat("Min Speed", &m_particleParams.minSpeed, 0.01f, 0.0f, 200.0f);
    ImGui::DragFloat("Max Speed", &m_particleParams.maxSpeed, 0.01f, 0.0f, 200.0f);
    if (m_particleParams.minSpeed > m_particleParams.maxSpeed)
    {
        std::swap(m_particleParams.minSpeed, m_particleParams.maxSpeed);
    }

    ImGui::DragFloat("Rotation Speed", &m_particleParams.startRotationSpeed, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Stretch Factor", &m_particleParams.stretchFactor, 0.01f, 0.0f, 20.0f);

    ImGui::Separator();
    ImGui::Text("Collision");
    static const char* kCollisionModes[] = { "None", "Bounce", "Stop", "Kill" };
    int collisionMode = static_cast<int>(m_collisionSettings.mode);
    if (ImGui::Combo("Collision Mode", &collisionMode, kCollisionModes, IM_ARRAYSIZE(kCollisionModes)))
    {
        m_collisionSettings.mode = static_cast<CollisionMode>(std::clamp(collisionMode, 0, kCollisionModeCount - 1));
    }

    if (m_collisionSettings.mode != CollisionMode::None)
    {
        ImGui::DragFloat("Collision Radius", &m_collisionSettings.particleRadius, 0.001f, 0.0f, 5.0f);
        ImGui::DragFloat("Restitution", &m_collisionSettings.restitution, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Collision Damping", &m_collisionSettings.damping, 0.01f, 0.0f, 1.0f);
    }

    ImGui::Separator();
    ImGui::Text("Sub Emitter");
    ImGui::Checkbox("Spawn On Death", &m_subEmitterSettings.spawnOnDeath);
    ImGui::Checkbox("Spawn On Collision", &m_subEmitterSettings.spawnOnCollision);
    int spawnCount = static_cast<int>(m_subEmitterSettings.spawnCount);
    if (ImGui::DragInt("Spawn Count", &spawnCount, 1, 0, 128))
    {
        m_subEmitterSettings.spawnCount = static_cast<UINT>(std::max(0, spawnCount));
    }

    int maxGeneration = static_cast<int>(m_subEmitterSettings.maxGeneration);
    if (ImGui::DragInt("Max Generation", &maxGeneration, 1, 0, 8))
    {
        m_subEmitterSettings.maxGeneration = static_cast<UINT>(std::max(0, maxGeneration));
    }

    ImGui::DragFloat("Inherit Velocity", &m_subEmitterSettings.inheritVelocity, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Sub Spread", &m_subEmitterSettings.spread, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Sub Min Speed", &m_subEmitterSettings.minSpeed, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Sub Max Speed", &m_subEmitterSettings.maxSpeed, 0.01f, 0.0f, 100.0f);
    if (m_subEmitterSettings.minSpeed > m_subEmitterSettings.maxSpeed)
    {
        std::swap(m_subEmitterSettings.minSpeed, m_subEmitterSettings.maxSpeed);
    }

    ImGui::DragFloat("Sub Min Lifetime", &m_subEmitterSettings.minLifetime, 0.01f, 0.01f, 50.0f);
    ImGui::DragFloat("Sub Max Lifetime", &m_subEmitterSettings.maxLifetime, 0.01f, 0.01f, 50.0f);
    if (m_subEmitterSettings.minLifetime > m_subEmitterSettings.maxLifetime)
    {
        std::swap(m_subEmitterSettings.minLifetime, m_subEmitterSettings.maxLifetime);
    }

    ImGui::DragFloat("Sub Size Scale", &m_subEmitterSettings.sizeScale, 0.01f, 0.01f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Render");
    static const char* kRenderModes[] = { "Billboard", "Stretched", "Horizontal", "Vertical" };
    int renderMode = static_cast<int>(m_renderSettings.mode);
    if (ImGui::Combo("Render Mode", &renderMode, kRenderModes, IM_ARRAYSIZE(kRenderModes)))
    {
        m_renderSettings.mode = static_cast<RenderMode>(std::clamp(renderMode, 0, kRenderModeCount - 1));
    }

    int rows = static_cast<int>(m_renderSettings.flipbookRows);
    int cols = static_cast<int>(m_renderSettings.flipbookCols);
    if (ImGui::DragInt("SubUV Rows", &rows, 1.0f, 1, 128))
    {
        m_renderSettings.flipbookRows = static_cast<UINT>(std::max(1, rows));
    }

    if (ImGui::DragInt("SubUV Cols", &cols, 1.0f, 1, 128))
    {
        m_renderSettings.flipbookCols = static_cast<UINT>(std::max(1, cols));
    }

    ImGui::DragFloat("SubUV FPS", &m_renderSettings.flipbookFps, 0.1f, 0.0f, 240.0f);
    ImGui::Checkbox("Random Start Frame", &m_renderSettings.randomStartFrame);

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
    ImGui::Text("Alive Particles: %u", m_aliveParticleCount);
    ImGui::Text("Frame Emit Count: %u", m_lastEmitCount);
}

void CpuParticleComponent::render()
{
    auto* cmd = DX12::Instance().getGraphicsCommandList();
    render(cmd);
}

void CpuParticleComponent::render(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
    {
        return;
    }

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    PSOCreator::Instance().setPSO(m_renderPSOKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());
    cmd->SetGraphicsRootConstantBufferView(1, m_renderCB->getGPUAddress());
    cmd->SetGraphicsRootDescriptorTable(2, DescriptorHeapManager::Instance().getGPUHandle(m_renderSrvTableBase));

    if (m_aliveParticleCount == 0)
    {
        return;
    }

    cmd->DrawInstanced(6, m_aliveParticleCount, 0, 0);
}

void CpuParticleComponent::renderForward(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !m_initialized)
    {
        return;
    }

    render(cmd);
}

void CpuParticleComponent::setTexture(const std::wstring& path)
{
    if (path.empty())
    {
        return;
    }

    m_texturePath = path;
    m_texture = TextureManager::Instance().load(path);
    updateDescriptorTables();
}

void CpuParticleComponent::setMaxParticles(UINT maxParticles)
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

    if (m_particlesCpu.size() > m_maxParticles)
    {
        m_particlesCpu.resize(m_maxParticles);
    }

    createParticleBuffer();
    updateDescriptorTables();
}

void CpuParticleComponent::initializeResources()
{
    m_renderCB = DXMem::makeUnique<ConstantBuffer<RenderParams>>(1);

    if (!m_texturePath.empty())
    {
        m_texture = TextureManager::Instance().load(m_texturePath);
    }

    createParticleBuffer();
    createPipelines();

    if (m_renderSrvTableBase == UINT_MAX)
    {
        m_renderSrvTableBase = DescriptorHeapManager::Instance().allocateRange(2);
    }

    updateDescriptorTables();
}

void CpuParticleComponent::createParticleBuffer()
{
    if (m_particleSrvIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_particleSrvIndex);
        m_particleSrvIndex = UINT_MAX;
    }

    m_particleBuffer.Reset();
    m_particleBufferMapped = nullptr;

    ID3D12Device* device = DX12::Instance().getDevice();
    const UINT64 bufferSize = std::max<UINT64>(1, sizeof(Particle) * m_maxParticles);

    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_particleBuffer.GetAddressOf()));
    LOG_HR(hr, "CpuParticle: create particle upload buffer failed");

    if (m_particleBuffer)
    {
        void* mapped = nullptr;
        hr = m_particleBuffer->Map(0, nullptr, &mapped);
        LOG_HR(hr, "CpuParticle: map particle upload buffer failed");
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

void CpuParticleComponent::syncParticleBuffer()
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

void CpuParticleComponent::createPipelines()
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
}

void CpuParticleComponent::updateDescriptorTables()
{
    if (m_renderSrvTableBase == UINT_MAX || !m_texture || m_particleSrvIndex == UINT_MAX)
    {
        return;
    }

    std::vector<UINT> srvIndices =
    {
        m_particleSrvIndex,
        m_texture->getSRVIndex()
    };

    DescriptorHeapManager::Instance().copyDescriptorsRange(m_renderSrvTableBase, srvIndices);
}

void CpuParticleComponent::appendSpawnedParticle(std::vector<Particle>& outParticles, const Particle& particle) const
{
    if (outParticles.size() >= m_maxParticles)
    {
        return;
    }

    outParticles.push_back(particle);
}

void CpuParticleComponent::spawnParticle(std::vector<Particle>& outParticles, UINT& seed, const Vector3& positionHint, const Vector3& directionHint, UINT generation) const
{
    Particle particle{};
    particle.position = positionHint;

    Vector3 velocityDir = randomDirectionAround(seed, directionHint, m_emitterParams.spread);
    const float speedT = nextRandom01(seed);
    const float speed = std::lerp(m_particleParams.minSpeed, m_particleParams.maxSpeed, speedT);

    particle.velocity = velocityDir * speed;
    particle.age = 0.0f;
    particle.lifetime = std::lerp(m_particleParams.minLifetime, m_particleParams.maxLifetime, nextRandom01(seed));
    particle.size = m_particleParams.startSize;
    particle.rotation = nextRandom01(seed) * XM_2PI;
    particle.rotationSpeed = (nextRandom01(seed) * 2.0f - 1.0f) * m_particleParams.startRotationSpeed;
    particle.stretch = m_particleParams.stretchFactor;
    particle.color = m_particleParams.startColor;
    particle.subUvStartFrame = 0.0f;
    particle.generation = generation;

    const UINT totalFrames = std::max(1u, m_renderSettings.flipbookRows * m_renderSettings.flipbookCols);
    if (m_renderSettings.randomStartFrame && totalFrames > 1)
    {
        particle.subUvStartFrame = std::floor(nextRandom01(seed) * static_cast<float>(totalFrames));
    }

    appendSpawnedParticle(outParticles, particle);
}

void CpuParticleComponent::emitSubParticles(const Particle& parent, const Vector3& normalHint, std::vector<Particle>& outParticles, UINT& seed)
{
    if (m_subEmitterSettings.spawnCount == 0)
    {
        return;
    }

    if (parent.generation >= m_subEmitterSettings.maxGeneration)
    {
        return;
    }

    for (UINT i = 0; i < m_subEmitterSettings.spawnCount && outParticles.size() < m_maxParticles; ++i)
    {
        Particle child{};
        child.position = parent.position;

        Vector3 axis = normalHint;
        if (axis.LengthSquared() <= 1e-6f)
        {
            axis = parent.velocity;
            if (axis.LengthSquared() <= 1e-6f)
            {
                axis = Vector3::UnitY;
            }
        }
        axis.Normalize();

        Vector3 dir = randomDirectionAround(seed, axis, m_subEmitterSettings.spread);
        const float speed = std::lerp(m_subEmitterSettings.minSpeed, m_subEmitterSettings.maxSpeed, nextRandom01(seed));
        child.velocity = parent.velocity * m_subEmitterSettings.inheritVelocity + dir * speed;

        child.age = 0.0f;
        child.lifetime = std::lerp(m_subEmitterSettings.minLifetime, m_subEmitterSettings.maxLifetime, nextRandom01(seed));
        child.size = std::max(0.001f, m_particleParams.startSize * m_subEmitterSettings.sizeScale);
        child.rotation = nextRandom01(seed) * XM_2PI;
        child.rotationSpeed = (nextRandom01(seed) * 2.0f - 1.0f) * m_particleParams.startRotationSpeed;
        child.stretch = m_particleParams.stretchFactor;
        child.color = m_particleParams.startColor;
        child.subUvStartFrame = m_renderSettings.randomStartFrame ? std::floor(nextRandom01(seed) * static_cast<float>(std::max(1u, m_renderSettings.flipbookRows * m_renderSettings.flipbookCols))) : 0.0f;
        child.generation = parent.generation + 1;

        appendSpawnedParticle(outParticles, child);
    }
}

bool CpuParticleComponent::resolveCollision(Particle& inOutParticle, const Vector3& previousPosition, float dt, Vector3& outHitNormal) const
{
    (void)dt;

    if (m_collisionSettings.mode == CollisionMode::None)
    {
        return false;
    }

    const Vector3 movement = inOutParticle.position - previousPosition;
    const float distance = movement.Length();
    if (distance <= 1e-6f)
    {
        return false;
    }

    Vector3 dir = movement;
    dir.Normalize();

    PhysicsWorld::RaycastHit hit{};
    if (!PhysicsWorld::Instance().raycast(previousPosition, dir, distance + m_collisionSettings.particleRadius, hit))
    {
        return false;
    }

    outHitNormal = hit.normal;
    if (outHitNormal.LengthSquared() <= 1e-6f)
    {
        outHitNormal = Vector3::UnitY;
    }
    outHitNormal.Normalize();

    inOutParticle.position = hit.point + outHitNormal * std::max(0.0f, m_collisionSettings.particleRadius);

    switch (m_collisionSettings.mode)
    {
    case CollisionMode::Bounce:
    {
        inOutParticle.velocity = Vector3::Reflect(inOutParticle.velocity, outHitNormal) * std::max(0.0f, m_collisionSettings.restitution);
        inOutParticle.velocity *= std::clamp(1.0f - m_collisionSettings.damping, 0.0f, 1.0f);
        break;
    }
    case CollisionMode::Stop:
        inOutParticle.velocity = Vector3::Zero;
        break;
    case CollisionMode::Kill:
        return true;
    case CollisionMode::None:
    default:
        break;
    }

    return false;
}

FbxRenderComponent* CpuParticleComponent::resolveMeshSourceRenderer() const
{
    if (m_meshSourceObjectName.empty())
    {
        return gameObject() ? gameObject()->getComponent<FbxRenderComponent>() : nullptr;
    }

    for (GameObject* candidate : GameObjectRegistry::Instance().getAll())
    {
        if (!candidate || candidate->isDestroyed())
        {
            continue;
        }

        if (candidate->getName() == m_meshSourceObjectName)
        {
            return candidate->getComponent<FbxRenderComponent>();
        }
    }

    return nullptr;
}

void CpuParticleComponent::rebuildMeshEmitterCache()
{
    m_meshTriangles.clear();
    m_meshTotalArea = 0.0f;

    FbxRenderComponent* renderer = resolveMeshSourceRenderer();
    if (!renderer || !renderer->getModel())
    {
        return;
    }

    const Model* model = renderer->getModel();
    const auto& modelData = model->getResource()->getModelData();
    const auto& runtimeBones = model->getBone();

    for (const auto& mesh : modelData.meshes)
    {
        if (mesh.indices.size() < 3 || mesh.vertices.empty())
        {
            continue;
        }

        if (mesh.nodeIndex < 0 || mesh.nodeIndex >= static_cast<int>(runtimeBones.size()))
        {
            continue;
        }

        const Matrix& world = runtimeBones[mesh.nodeIndex].worldTransform;

        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            const uint32_t i0 = mesh.indices[i + 0];
            const uint32_t i1 = mesh.indices[i + 1];
            const uint32_t i2 = mesh.indices[i + 2];

            if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
            {
                continue;
            }

            const Vector3 p0 = Vector3::Transform(mesh.vertices[i0].position, world);
            const Vector3 p1 = Vector3::Transform(mesh.vertices[i1].position, world);
            const Vector3 p2 = Vector3::Transform(mesh.vertices[i2].position, world);

            const Vector3 edge1 = p1 - p0;
            const Vector3 edge2 = p2 - p0;
            Vector3 n = edge1.Cross(edge2);
            const float areaTwice = n.Length();
            if (areaTwice <= 1e-7f)
            {
                continue;
            }

            n *= safeInv(areaTwice);
            const float triArea = areaTwice * 0.5f;

            m_meshTotalArea += triArea;

            SurfaceTriangle tri{};
            tri.a = p0;
            tri.b = p1;
            tri.c = p2;
            tri.normal = n;
            tri.cumulativeArea = m_meshTotalArea;
            m_meshTriangles.push_back(tri);
        }
    }
}

bool CpuParticleComponent::ensureMeshEmitterCache()
{
    m_meshCacheAge += TimeManager::Instance().getDeltaTime();

    if (m_meshTriangles.empty() || m_meshCacheAge >= std::max(0.01f, m_emitterParams.meshCacheUpdateInterval))
    {
        rebuildMeshEmitterCache();
        m_meshCacheAge = 0.0f;
    }

    return !m_meshTriangles.empty() && m_meshTotalArea > 1e-6f;
}

bool CpuParticleComponent::sampleMeshSurface(UINT& seed, SurfaceSample& outSample) const
{
    if (m_meshTriangles.empty() || m_meshTotalArea <= 1e-6f)
    {
        return false;
    }

    const float target = nextRandom01(seed) * m_meshTotalArea;

    const auto it = std::lower_bound(
        m_meshTriangles.begin(),
        m_meshTriangles.end(),
        target,
        [](const SurfaceTriangle& tri, float value)
        {
            return tri.cumulativeArea < value;
        });

    if (it == m_meshTriangles.end())
    {
        return false;
    }

    const float r1 = nextRandom01(seed);
    const float r2 = nextRandom01(seed);
    const float sqrtR1 = std::sqrt(r1);

    const float u = 1.0f - sqrtR1;
    const float v = sqrtR1 * (1.0f - r2);
    const float w = sqrtR1 * r2;

    outSample.position = it->a * u + it->b * v + it->c * w;
    outSample.normal = it->normal;
    return true;
}

void CpuParticleComponent::simulateParticles(float dt)
{
    if (m_maxParticles == 0)
    {
        m_aliveParticleCount = 0;
        m_particlesCpu.clear();
        return;
    }

    if (m_emitterParams.type == EmitterType::MeshSurface)
    {
        ensureMeshEmitterCache();
    }

    std::vector<Particle> nextParticles;
    nextParticles.reserve(m_maxParticles);

    UINT seed = m_randomSeed;

    for (const auto& src : m_particlesCpu)
    {
        Particle particle = src;
        const Vector3 prevPos = particle.position;

        particle.age += dt;
        if (particle.age >= particle.lifetime)
        {
            if (m_subEmitterSettings.spawnOnDeath)
            {
                Vector3 normal = particle.velocity;
                if (normal.LengthSquared() <= 1e-6f)
                {
                    normal = Vector3::UnitY;
                }
                normal.Normalize();
                emitSubParticles(particle, normal, nextParticles, seed);
            }
            continue;
        }

        particle.velocity += m_particleParams.gravity * dt;
        particle.velocity *= std::max(0.0f, 1.0f - m_particleParams.drag * dt);
        particle.position += particle.velocity * dt;

        Vector3 hitNormal = Vector3::Zero;
        const bool killedByCollision = resolveCollision(particle, prevPos, dt, hitNormal);
        if (killedByCollision)
        {
            if (m_subEmitterSettings.spawnOnCollision)
            {
                emitSubParticles(particle, hitNormal, nextParticles, seed);
            }
            continue;
        }

        if (hitNormal.LengthSquared() > 1e-6f && m_subEmitterSettings.spawnOnCollision)
        {
            emitSubParticles(particle, hitNormal, nextParticles, seed);
        }

        const float t = std::clamp(particle.age / std::max(0.0001f, particle.lifetime), 0.0f, 1.0f);
        particle.size = std::lerp(m_particleParams.startSize, m_particleParams.endSize, t);
        particle.color = Vector4(
            std::lerp(m_particleParams.startColor.x, m_particleParams.endColor.x, t),
            std::lerp(m_particleParams.startColor.y, m_particleParams.endColor.y, t),
            std::lerp(m_particleParams.startColor.z, m_particleParams.endColor.z, t),
            std::lerp(m_particleParams.startColor.w, m_particleParams.endColor.w, t));
        particle.rotation += particle.rotationSpeed * dt;

        appendSpawnedParticle(nextParticles, particle);
    }

    for (UINT i = 0; i < m_lastEmitCount && nextParticles.size() < m_maxParticles; ++i)
    {
        Vector3 spawnPos = m_emitOrigin;
        Vector3 spawnNormal = m_emitterParams.emitDirection;

        if (m_emitterParams.type == EmitterType::Sphere)
        {
            spawnPos = m_emitOrigin + randomSpherePoint(seed, std::max(0.0f, m_emitterParams.emitRadius));
            spawnNormal = spawnPos - m_emitOrigin;
            if (spawnNormal.LengthSquared() <= 1e-6f)
            {
                spawnNormal = m_emitterParams.emitDirection;
            }
        }
        else if (m_emitterParams.type == EmitterType::MeshSurface)
        {
            SurfaceSample sample{};
            if (sampleMeshSurface(seed, sample))
            {
                spawnPos = sample.position;
                spawnNormal = sample.normal;
            }
            else
            {
                spawnPos = m_emitOrigin;
                spawnNormal = m_emitterParams.emitDirection;
            }
        }

        spawnParticle(nextParticles, seed, spawnPos, spawnNormal, 0);
    }

    m_particlesCpu.swap(nextParticles);
    m_aliveParticleCount = static_cast<UINT>(m_particlesCpu.size());
}