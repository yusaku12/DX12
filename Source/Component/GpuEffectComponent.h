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
    void awake() override;

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

    //! GPU シミュレーション
    void simulate(ID3D12GraphicsCommandList* cmd);

    //! テクスチャ設定
    void setTexture(const std::wstring& path);

    //! 最大粒子数設定
    void setMaxParticles(UINT maxParticles);

private:
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
    };

    struct SimParams
    {
        float deltaTime = 0.0f;
        float totalTime = 0.0f;
        float emitRate = 0.0f;
        UINT emitCount = 0;

        UINT maxParticles = 0;
        float lifetime = 0.0f;
        float speed = 0.0f;
        float spread = 0.0f;

        float startSize = 0.0f;
        float endSize = 0.0f;
        float drag = 0.0f;
        UINT emitterType = 0;

        Vector3 emitOrigin = Vector3::Zero;
        float emitRadius = 0.0f;

        Vector4 startColor = Vector4::One;
        Vector4 endColor = Vector4::One;

        Vector3 gravity = Vector3::Zero;
        float noiseStrength = 0.0f;

        Vector3 emitterSize = Vector3::One;
        float noiseFrequency = 1.0f;

        float coneAngle = 30.0f;
        float coneHeight = 1.0f;
        float minLifetime = 1.0f;
        float maxLifetime = 2.0f;

        float minSpeed = 1.0f;
        float maxSpeed = 3.0f;
        float startRotationSpeed = 0.0f;
        float stretchFactor = 0.0f;

        UINT renderMode = 0;
        UINT flipbookRows = 1;
        UINT flipbookCols = 1;
        float flipbookFps = 0.0f;

        UINT randomSeed = 0;
        UINT _pad0 = 0;
        UINT _pad1 = 0;
        UINT _pad2 = 0;
    };

    struct ParticleBuffer
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> counter;
        UINT srvIndex = UINT_MAX;
        UINT uavIndex = UINT_MAX;
    };

    //! 初期化
    void initializeResources();

    //! パーティクルバッファ生成
    void createParticleBuffer(int index);

    //! CPU 粒子を GPU バッファへ反映
    void syncParticleBuffer();

    //! CPU 側で粒子を更新
    void simulateParticles(float dt);

    //! 描画引数バッファ生成
    void createDrawArgsBuffer();

    //! カウントバッファ生成
    void createAliveCountBuffer();

    //! RootSignature/PSO生成
    void createPipelines();

    //! ディスクリプタテーブル更新
    void updateDescriptorTables();

    //! デバッグログ出力判定
    bool shouldOutputDebugLog();

    LoadTexture* m_texture = nullptr;
    std::wstring m_texturePath = L"__white__";

    UINT m_maxParticles = 20000;
    float m_emitRate = 400.0f;
    float m_emitAccumulator = 0.0f;
    float m_lifetime = 1.5f;
    float m_speed = 2.0f;
    float m_spread = 0.6f;
    float m_startSize = 0.2f;
    float m_endSize = 0.8f;
    float m_drag = 0.0f;
    UINT m_emitterType = 0; // 0: Sphere, 1: Box, 2: Cone, 3: Ring
    float m_emitRadius = 1.0f;
    Vector4 m_startColor = Vector4(1, 1, 1, 1);
    Vector4 m_endColor = Vector4(1, 1, 1, 0);

    Vector3 m_gravity = Vector3(0.0f, -9.8f, 0.0f);
    float m_noiseStrength = 0.0f;
    Vector3 m_emitterSize = Vector3(1.0f, 1.0f, 1.0f);
    float m_noiseFrequency = 1.0f;

    float m_coneAngle = 30.0f;
    float m_coneHeight = 1.0f;
    float m_minLifetime = 1.0f;
    float m_maxLifetime = 2.0f;

    float m_minSpeed = 1.0f;
    float m_maxSpeed = 3.0f;
    float m_startRotationSpeed = 0.0f;
    float m_stretchFactor = 0.0f;

    UINT m_renderMode = 0;
    UINT m_flipbookRows = 1;
    UINT m_flipbookCols = 1;
    float m_flipbookFps = 0.0f;

    std::unique_ptr<ConstantBuffer<SimParams>> m_simCB;
    SimParams m_simParams;

    std::vector<Particle> m_particlesCpu;

    ParticleBuffer m_particles[2];
    int m_currentIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffer;
    Particle* m_particleBufferMapped = nullptr;
    UINT m_particleSrvIndex = UINT_MAX;
    UINT m_aliveParticleCount = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_aliveCountBuffer;
    UINT m_aliveCountSrvIndex = UINT_MAX;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_drawArgsBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_drawArgsUpload;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_drawCommandSignature;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_counterResetUpload;

    UINT m_computeUavTableBase = UINT_MAX;
    UINT m_renderSrvTableBase = UINT_MAX;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_computePSO;
    size_t m_renderPSOKey = 0;

    D3D12_RESOURCE_STATES m_particleStates[2] = { D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON };
    D3D12_RESOURCE_STATES m_counterStates[2] = { D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON };
    D3D12_RESOURCE_STATES m_aliveCountState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_drawArgsState = D3D12_RESOURCE_STATE_COMMON;

    bool m_initialized = false;
    bool m_countersInitialized = false;

    bool m_enableDebugLog = false; //!< デバッグログ有効
    int m_debugLogInterval = 60;  //!< ログ間隔（フレーム）
    int m_debugFrameCounter = 0;  //!< フレームカウンタ
};