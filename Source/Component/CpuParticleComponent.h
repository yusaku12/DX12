#pragma once

#include "Component/IRenderComponent.h"
#include "Graphics/ConstantBuffer.h"

class LoadTexture;
class FbxRenderComponent;

//=====================================================
//! CPU パーティクルコンポーネント
//=====================================================
class CpuParticleComponent : public IRenderComponent
{
public:
    ~CpuParticleComponent() override = default;

    void awake() override;
    void start() override;
    void update() override;
    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;
    void inspectGUI() override;

    void render() override;
    void render(ID3D12GraphicsCommandList* cmd) override;
    void renderForward(ID3D12GraphicsCommandList* cmd) override;

    void setTexture(const std::wstring& path);
    void setMaxParticles(UINT maxParticles);
    void setMeshSourceObjectName(const std::string& objectName) { m_meshSourceObjectName = objectName; }
    void setEmitterType(UINT emitterType) { m_emitterParams.type = static_cast<EmitterType>(std::clamp(emitterType, 0u, static_cast<UINT>(kEmitterTypeCount - 1))); }
    void setEmitRate(float emitRate) { m_emitterParams.emitRate = std::max(0.0f, emitRate); }
    void setEmitRadius(float radius) { m_emitterParams.emitRadius = std::max(0.0f, radius); }
    void setCollisionMode(UINT mode) { m_collisionSettings.mode = static_cast<CollisionMode>(std::clamp(mode, 0u, static_cast<UINT>(kCollisionModeCount - 1))); }
    void setSubUvRows(UINT rows) { m_renderSettings.flipbookRows = std::max(1u, rows); }
    void setSubUvCols(UINT cols) { m_renderSettings.flipbookCols = std::max(1u, cols); }
    void setSubUvFps(float fps) { m_renderSettings.flipbookFps = std::max(0.0f, fps); }

    const std::wstring& getTexturePath() const { return m_texturePath; }
    UINT getMaxParticles() const { return m_maxParticles; }
    const std::string& getMeshSourceObjectName() const { return m_meshSourceObjectName; }
    UINT getEmitterType() const { return static_cast<UINT>(m_emitterParams.type); }
    float getEmitRate() const { return m_emitterParams.emitRate; }
    float getEmitRadius() const { return m_emitterParams.emitRadius; }
    UINT getCollisionMode() const { return static_cast<UINT>(m_collisionSettings.mode); }
    UINT getSubUvRows() const { return m_renderSettings.flipbookRows; }
    UINT getSubUvCols() const { return m_renderSettings.flipbookCols; }
    float getSubUvFps() const { return m_renderSettings.flipbookFps; }

private:
    enum class EmitterType : UINT
    {
        Point = 0,
        Sphere,
        MeshSurface,
    };

    enum class RenderMode : UINT
    {
        Billboard = 0,
        Stretched,
        Horizontal,
        Vertical,
    };

    enum class CollisionMode : UINT
    {
        None = 0,
        Bounce,
        Stop,
        Kill,
    };

    static constexpr int kEmitterTypeCount = 3;
    static constexpr int kRenderModeCount = 4;
    static constexpr int kCollisionModeCount = 4;

    struct Particle
    {
        Vector3 position = Vector3::Zero;
        float age = 0.0f;
        Vector3 velocity = Vector3::Zero;
        float lifetime = 0.0f;
        float size = 0.0f;
        float rotation = 0.0f;
        float rotationSpeed = 0.0f;
        float stretch = 1.0f;
        Vector4 color = Vector4::One;
        float subUvStartFrame = 0.0f;
        UINT generation = 0;
        UINT padding0 = 0;
        UINT padding1 = 0;
    };

    struct RenderParams
    {
        UINT renderMode = 0;
        UINT flipbookRows = 1;
        UINT flipbookCols = 1;
        float flipbookFps = 0.0f;
    };

    struct EmitterParams
    {
        EmitterType type = EmitterType::Point;
        float emitRate = 300.0f;
        float spread = 0.6f;
        float emitRadius = 1.0f;
        Vector3 emitDirection = Vector3::UnitY;
        float meshCacheUpdateInterval = 0.2f;
    };

    struct ParticleParams
    {
        float startSize = 0.2f;
        float endSize = 0.8f;
        Vector4 startColor = Vector4(1, 1, 1, 1);
        Vector4 endColor = Vector4(1, 1, 1, 0);

        Vector3 gravity = Vector3(0.0f, -9.8f, 0.0f);
        float drag = 0.0f;

        float minLifetime = 1.0f;
        float maxLifetime = 2.0f;
        float minSpeed = 1.0f;
        float maxSpeed = 3.0f;
        float startRotationSpeed = 0.0f;
        float stretchFactor = 1.0f;
    };

    struct RenderSettings
    {
        RenderMode mode = RenderMode::Billboard;
        UINT flipbookRows = 1;
        UINT flipbookCols = 1;
        float flipbookFps = 0.0f;
        bool randomStartFrame = true;
    };

    struct CollisionSettings
    {
        CollisionMode mode = CollisionMode::None;
        float particleRadius = 0.05f;
        float restitution = 0.6f;
        float damping = 0.05f;
    };

    struct SubEmitterSettings
    {
        bool spawnOnDeath = false;
        bool spawnOnCollision = true;
        UINT spawnCount = 4;
        UINT maxGeneration = 1;
        float inheritVelocity = 0.35f;
        float spread = 1.0f;
        float minSpeed = 0.5f;
        float maxSpeed = 2.5f;
        float minLifetime = 0.2f;
        float maxLifetime = 0.7f;
        float sizeScale = 0.5f;
    };

    struct SurfaceSample
    {
        Vector3 position = Vector3::Zero;
        Vector3 normal = Vector3::UnitY;
    };

    struct SurfaceTriangle
    {
        Vector3 a = Vector3::Zero;
        Vector3 b = Vector3::Zero;
        Vector3 c = Vector3::Zero;
        Vector3 normal = Vector3::UnitY;
        float cumulativeArea = 0.0f;
    };

    void initializeResources();
    void createParticleBuffer();
    void syncParticleBuffer();
    void createPipelines();
    void updateDescriptorTables();

    void simulateParticles(float dt);
    void appendSpawnedParticle(std::vector<Particle>& outParticles, const Particle& particle) const;
    void spawnParticle(std::vector<Particle>& outParticles, UINT& seed, const Vector3& positionHint, const Vector3& directionHint, UINT generation) const;
    void emitSubParticles(const Particle& parent, const Vector3& normalHint, std::vector<Particle>& outParticles, UINT& seed);

    bool resolveCollision(Particle& inOutParticle, const Vector3& previousPosition, float dt, Vector3& outHitNormal) const;

    bool ensureMeshEmitterCache();
    void rebuildMeshEmitterCache();
    bool sampleMeshSurface(UINT& seed, SurfaceSample& outSample) const;
    FbxRenderComponent* resolveMeshSourceRenderer() const;

    LoadTexture* m_texture = nullptr;
    std::wstring m_texturePath = L"__white__";

    UINT m_maxParticles = 20000;
    float m_emitAccumulator = 0.0f;
    EmitterParams m_emitterParams;
    ParticleParams m_particleParams;
    RenderSettings m_renderSettings;
    CollisionSettings m_collisionSettings;
    SubEmitterSettings m_subEmitterSettings;

    std::unique_ptr<ConstantBuffer<RenderParams>> m_renderCB;
    RenderParams m_renderParams;

    Vector3 m_emitOrigin = Vector3::Zero;
    float m_totalTime = 0.0f;
    UINT m_lastEmitCount = 0;
    UINT m_randomSeed = 0;

    std::vector<Particle> m_particlesCpu;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffer;
    Particle* m_particleBufferMapped = nullptr;
    UINT m_particleSrvIndex = UINT_MAX;
    UINT m_aliveParticleCount = 0;

    UINT m_renderSrvTableBase = UINT_MAX;
    size_t m_renderPSOKey = 0;

    bool m_initialized = false;

    mutable std::vector<SurfaceTriangle> m_meshTriangles;
    mutable float m_meshTotalArea = 0.0f;
    mutable float m_meshCacheAge = 0.0f;
    std::string m_meshSourceObjectName;
};
