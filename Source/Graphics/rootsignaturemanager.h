#pragma once

//------------------------------------------------
//! RootSignature の種類
//------------------------------------------------
enum class RootSignatureType : int
{
    FBXStandard,
    GBuffer,
    DebugPrimitive,
    PostEffect,
    PostEffectDepth,
    BloomComposite,
    Skybox,
    DeferredLighting,
    GpuEffectRender,
    GpuEffectCompute,
    HiZPyramidCompute,
    ShadowDepth,
    Max
};

//------------------------------------------------
//! 固定CBVの種類
//------------------------------------------------
enum class CBVType : int
{
    Camera,
};

//====================================================
// RootSignature管理シングルトン
//====================================================
class RootSignatureManager
{
public:

    //! シングルトンインスタンス取得
    static RootSignatureManager& Instance()
    {
        static RootSignatureManager instance;
        return instance;
    }

    //! 初期化
    void initialize();

    //! 指定した名前の RootSignature を取得
    ID3D12RootSignature* getRootSignature(RootSignatureType type) const;

private:

    RootSignatureManager() = default;

    //! FBXStandardをビルド
    void buildFBXStandard();

    //! GBufferをビルド
    void buildGBuffer();

    //! DebugPrimitiveをビルド
    void buildDebugPrimitive();

    //! PostEffectをビルド
    void buildPostEffect();

    //! PostEffectDepthをビルド
    void buildPostEffectDepth();

    //! BloomCompositeをビルド
    void buildBloomComposite();

    //! Skyboxをビルド
    void buildSkybox();

    //! DeferredLightingをビルド
    void buildDeferredLighting();

    //! GpuEffectRenderをビルド
    void buildGpuEffectRender();

    //! GpuEffectComputeをビルド
    void buildGpuEffectCompute();

    //! HiZPyramidComputeをビルド
    void buildHiZPyramidCompute();

    //! ShadowDepthをビルド（シャドウ深度パス用）
    void buildShadowDepth();

    //! 共通: FBX/GBuffer 系（CBV3 + SRVテーブル）
    void buildModelMaterialSRV(UINT srvCount, RootSignatureType type);

    //! 共通: PostEffect 系（CBV1 + SRVテーブル）
    void buildPostEffectCommon(bool useDepth);

    //! 共通: RootSignature をビルド（標準サンプラー 6 枚）
    void createRootSignature(const CD3DX12_ROOT_PARAMETER* params, UINT paramCount, RootSignatureType type);

    //! 共通: RootSignature をビルド（標準サンプラー + シャドウ比較サンプラー）
    void createRootSignatureWithShadowSampler(const CD3DX12_ROOT_PARAMETER* params, UINT paramCount, RootSignatureType type);

    RootSignatureType m_buildingType = RootSignatureType::Max;
    std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, static_cast<size_t>(RootSignatureType::Max)> m_rootSignatures;
};