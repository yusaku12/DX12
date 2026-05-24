#pragma once

#include "Component/IRenderComponent.h"
#include "Graphics/ConstantBuffer.h"

class LoadTexture;

//=====================================================
//! GPU エフェクト（Compute + Indirect Draw）コンポーネント
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
        Vector2 padding = Vector2::Zero;
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
        Vector2 padding0 = Vector2::Zero;

        Vector3 emitOrigin = Vector3::Zero;
        float emitRadius = 0.0f;

        Vector4 startColor = Vector4::One;
        Vector4 endColor = Vector4::One;
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
    float m_emitRadius = 0.0f;
    Vector4 m_startColor = Vector4(1, 1, 1, 1);
    Vector4 m_endColor = Vector4(1, 1, 1, 0);

    std::unique_ptr<ConstantBuffer<SimParams>> m_simCB;
    SimParams m_simParams;

    ParticleBuffer m_particles[2];
    int m_currentIndex = 0;

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

    bool m_debugForceDraw = false;       //!< 直接描画で確認する
    bool m_debugForceIndirectArgs = false; //!< DrawArgs を強制上書きする
};