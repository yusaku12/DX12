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
    };

    //! 描画用定数バッファ構造体
    struct RenderParams
    {
        UINT renderMode = 0;
        UINT flipbookRows = 1;
        UINT flipbookCols = 1;
        float flipbookFps = 0.0f;
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
    };

    //! 初期化
    void initializeResources();

    //! パーティクルバッファ生成
    void createParticleBuffer();

    //! CPU 粒子を GPU バッファへ反映
    void syncParticleBuffer();

    //! CPU 側で粒子を更新
    void simulateParticles(float dt);

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
};
