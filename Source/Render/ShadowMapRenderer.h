#pragma once

//=====================================================
//! Cascaded Shadow Maps (CSM) レンダラー
//! Unity / Unreal 準拠の 4 カスケード方式
//=====================================================
class ShadowMapRenderer
{
public:

    static constexpr int   CascadeCount  = 4;        //!< カスケード数
    static constexpr UINT  ShadowMapSize = 2048;      //!< カスケードごとの解像度
    static constexpr float CascadeLambda = 0.75f;     //!< λ ブレンド係数（対数 vs 一様分割）

    //! シングルトンインスタンス取得
    static ShadowMapRenderer& Instance()
    {
        static ShadowMapRenderer instance;
        return instance;
    }

    //! 初期化（window.cpp の DeferredRenderer::initialize() 直後に呼ぶ）
    void initialize();

    //! フレーム更新（カスケード計算 + CBV 更新）
    //! @param lightDir  ワールド空間の光源方向（正規化済みでなくてもよい）
    void update(const Vector3& lightDir);

    //! 指定カスケードの深度パスを開始（ビューポート・DSV セット + クリア）
    void beginCascadePass(ID3D12GraphicsCommandList* cmd, int cascade);

    //! 深度テクスチャを SRV 用ステートへ遷移
    void transitionToSRV(ID3D12GraphicsCommandList* cmd);

    //! 深度テクスチャを DSV 用ステートへ遷移
    void transitionToDSV(ID3D12GraphicsCommandList* cmd);

    //! カスケード光源行列 CBV の GPU アドレス（per cascade）
    D3D12_GPU_VIRTUAL_ADDRESS getLightVPCBAddress(int cascade) const;

    //! シャドウパラメータ CBV の GPU アドレス（DeferredLighting に渡す）
    D3D12_GPU_VIRTUAL_ADDRESS getShadowParamsCBAddress() const;

    //! シャドウマップ SRV のヒープハンドル（DeferredLighting に渡す）
    D3D12_GPU_DESCRIPTOR_HANDLE getShadowMapSRVHandle() const;

    //! 各カスケードの OBB (Light Space 境界をワールド空間へ変換したもの) を取得
    const DirectX::BoundingOrientedBox& getCascadeOBB(int cascade) const { return m_cascadeOBBs[cascade]; }

    //! 光源方向の設定
    void setLightDirection(const Vector3& dir) { m_lightDir = dir; }

    //! 深度バイアスの設定
    void setShadowBias(float bias)         { m_shadowParams.shadowBias     = bias; }

    //! 影強度の設定 [0, 1]
    void setShadowStrength(float strength) { m_shadowParams.shadowStrength = strength; }

    //! ImGui デバッグ UI
    void debugImGui();

private:

    ShadowMapRenderer()  = default;
    ~ShadowMapRenderer() = default;

    //=====================================================
    //! 定数バッファ構造体（HLSL との整合性を維持すること）
    //=====================================================

    //! 1 カスケードの光源 VP 行列（ShadowDepthVS の b0）
    struct ShadowLightCB
    {
        Matrix lightViewProj;       //!< このカスケードの光源 VP 行列
        float  cascadeIndex = 0.0f;
        float  pad[3]       = {};
    };

    //! DeferredLighting に渡すシャドウパラメータ全体（b2）
    struct ShadowParamsCB
    {
        Matrix  lightViewProj[CascadeCount];  //!< 全カスケードの光源 VP 行列
        Vector4 cascadeSplits;                //!< ビュー空間カスケード分割距離（正値）
        float   shadowBias      = 0.002f;
        float   shadowStrength  = 1.0f;
        float   shadowPadding[2]= {};
    };

    //=====================================================
    //! 内部メソッド
    //=====================================================

    void createResources();
    void computeCascades();

    static Matrix buildLightView(const Vector3& center, const Vector3& lightDir);

    //=====================================================
    //! メンバ変数
    //=====================================================

    Microsoft::WRL::ComPtr<ID3D12Resource>       m_shadowTexture;             //!< 深度テクスチャ配列
    D3D12_RESOURCE_STATES                        m_textureState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;                   //!< DSV ヒープ（CPU 専用）
    D3D12_CPU_DESCRIPTOR_HANDLE                  m_dsvHandles[CascadeCount]   = {};

    UINT m_srvIndex = UINT_MAX;                                                //!< DescriptorHeapManager 内の SRV インデックス

    //! カスケードごとの光源 VP 定数バッファ
    std::unique_ptr<ConstantBuffer<ShadowLightCB>> m_lightVPCBs[CascadeCount];

    //! シャドウパラメータ全体の定数バッファ
    std::unique_ptr<ConstantBuffer<ShadowParamsCB>> m_shadowParamsCB;

    ShadowParamsCB m_shadowParams{};  //!< CPU 側パラメータキャッシュ

    Vector3 m_lightDir = Vector3(0.3f, -1.0f, 0.5f);  //!< 光源方向（normalize せずに渡してもよい）

    DirectX::BoundingOrientedBox m_cascadeOBBs[CascadeCount] = {}; //!< 各カスケードの境界 OBB (ワールド空間)
};
