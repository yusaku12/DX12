#pragma once

#include "Component/IRenderComponent.h"
#include "Graphics/ConstantBuffer.h"

class LoadTexture;

//=====================================================
//! GPU エフェクトコンポーネント
//=====================================================
class GpuEffectComponent : public IRenderComponent
{
public:
    ~GpuEffectComponent() override = default;

    //! 初期化
    void awake() override {};

    //! ゲーム開始時
    void start() override;

    //! 更新
    void update() override;

    //! 有効化
    void onEnable() override;

    //! 無効化
    void onDisable() override;

    //! 破棄
    void onDestroy() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! 描画(シングルスレッド)
    void render() override;

    //! 描画(マルチスレッド)
    void render(ID3D12GraphicsCommandList* cmd) override;

    //! Forward 描画
    void renderForward(ID3D12GraphicsCommandList* cmd) override;

    //! テクスチャ設定
    void setTexture(const std::wstring& path);

    //! 最大粒子数設定
    void setMaxParticles(UINT maxParticles);

    //! 保存用取得API
    const std::wstring& getTexturePath() const { return m_texturePath; }
    UINT getMaxParticles() const { return m_maxParticles; }

private:
    enum class EmitterType : UINT
    {
        Sphere = 0,
        Box,
        Cone,
        Ring,
    };

    enum class RenderMode : UINT
    {
        Billboard = 0,
        Stretched,
        Horizontal,
        Vertical,
    };

    static constexpr int kEmitterTypeCount = 4;
    static constexpr int kRenderModeCount = 4;

    //! GPU/CPU 両方で管理する粒子構造体
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

    //! 描画用定数バッファ構造体
    struct RenderParams
    {
        UINT renderMode = 0;
        UINT flipbookRows = 1;
        UINT flipbookCols = 1;
        float flipbookFps = 0.0f;
        float graphId = 0.0f;
        float metallic = 0.0f;
        float roughness = 1.0f;
        float ao = 1.0f;
    };

    //! GPUシミュレーション定数バッファ
    struct SimParams
    {
        float deltaTime = 0.0f;
        float totalTime = 0.0f;
        float emitRate = 0.0f;
        float emitRadius = 0.0f;

        UINT maxParticles = 0;
        UINT emitterType = 0;
        UINT randomSeed = 0;
        UINT resetAll = 0;

        float spread = 0.0f;
        float coneAngle = 0.0f;
        float coneHeight = 0.0f;
        float pad0 = 0.0f;

        Vector3 emitterSize = Vector3::One;
        float pad1 = 0.0f;

        Vector3 emitOrigin = Vector3::Zero;
        float pad2 = 0.0f;

        float minLifetime = 0.0f;
        float maxLifetime = 0.0f;
        float minSpeed = 0.0f;
        float maxSpeed = 0.0f;

        float startSize = 0.0f;
        float endSize = 0.0f;
        float drag = 0.0f;
        float startRotationSpeed = 0.0f;

        float stretchFactor = 0.0f;
        float noiseStrength = 0.0f;
        float noiseFrequency = 0.0f;
        float pad3 = 0.0f;

        Vector3 gravity = Vector3::Zero;
        float pad4 = 0.0f;

        Vector4 startColor = Vector4::One;
        Vector4 endColor = Vector4::One;
    };

    //! エミッタ設定
    struct EmitterParams
    {
        float emitRate = 400.0f;
        float spread = 0.6f;
        float emitRadius = 1.0f;
        EmitterType type = EmitterType::Sphere;
        Vector3 emitterSize = Vector3(1.0f, 1.0f, 1.0f);
        float coneAngle = 30.0f;
        float coneHeight = 1.0f;
    };

    //! 粒子挙動設定
    struct ParticleParams
    {
        float startSize = 0.2f;
        float endSize = 0.8f;
        Vector4 startColor = Vector4(1, 1, 1, 1);
        Vector4 endColor = Vector4(1, 1, 1, 0);

        Vector3 gravity = Vector3(0.0f, -9.8f, 0.0f);
        float drag = 0.0f;
        float noiseStrength = 0.0f;
        float noiseFrequency = 1.0f;

        float minLifetime = 1.0f;
        float maxLifetime = 2.0f;
        float minSpeed = 1.0f;
        float maxSpeed = 3.0f;
        float startRotationSpeed = 0.0f;
        float stretchFactor = 1.0f;
    };

    //! 描画設定
    struct RenderSettings
    {
        RenderMode mode = RenderMode::Billboard;
        UINT flipbookRows = 1;
        UINT flipbookCols = 1;
        float flipbookFps = 0.0f;
        int graphId = 0;
        float metallic = 0.0f;
        float roughness = 1.0f;
        float ao = 1.0f;
    };

    //! 初期化
    void initializeResources();

    //! パーティクルバッファ生成
    void createParticleBuffers();

    //! GPU駆動描画用リソース作成
    void createGpuDrivenDrawResources();

    //! GPUシミュレーション実行
    void runGpuSimulation(ID3D12GraphicsCommandList* cmd);

    //! シミュレーション定数バッファ更新
    void refreshSimulationParams(float deltaTime);

    //! RootSignature/PSO生成
    void createPipelines();

    //! ディスクリプタテーブル更新
    void updateDescriptorTables();

    LoadTexture* m_texture = nullptr;
    std::wstring m_texturePath = L"__white__";

    UINT m_maxParticles = 20000;
    float m_emitAccumulator = 0.0f;
    EmitterParams m_emitterParams;
    ParticleParams m_particleParams;
    RenderSettings m_renderSettings;

    std::unique_ptr<ConstantBuffer<RenderParams>> m_renderCB;
    std::unique_ptr<ConstantBuffer<SimParams>> m_simCB;
    RenderParams m_renderParams;
    SimParams m_simParams;

    Vector3 m_emitOrigin = Vector3::Zero;
    float m_totalTime = 0.0f;
    UINT m_randomSeed = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffers[2];
    UINT m_particleSrvIndices[2] = { UINT_MAX, UINT_MAX };
    UINT m_particleUavIndices[2] = { UINT_MAX, UINT_MAX };
    D3D12_RESOURCE_STATES m_particleStates[2] =
    {
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COMMON
    };
    UINT m_readBufferIndex = 0;
    UINT m_aliveParticleCount = 0;

    UINT m_renderSrvTableBase = UINT_MAX;
    UINT m_computeTableBase = UINT_MAX;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_drawArgsBuffer;
    UINT m_drawArgsUavIndex = UINT_MAX;
    D3D12_RESOURCE_STATES m_drawArgsState = D3D12_RESOURCE_STATE_COMMON;

    size_t m_renderPSOKey = 0;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_computePSO;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_drawCommandSignature;

    bool m_initialized = false;
    bool m_resetSimulation = true;
};
