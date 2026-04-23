#pragma once

#include "PostEffectBase.h"

//=====================================================
//! Bloom エフェクト（UE4 / COD 方式 ダウン＋アップサンプルピラミッド）
//!
//! パス構成（MIP_COUNT = 4 の場合）:
//!   Prefilter     : フル → 1/2
//!   Downsample x4 : 1/2 → 1/4 → 1/8 → 1/16
//!   Upsample   x4 : 1/16 → 1/8 → 1/4 → 1/2
//!   Composite     : 元シーン + 1/2 を加算合成
//=====================================================
class BloomEffect : public PostEffectBase
{
public:

    BloomEffect() { m_priority = 50; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;
    const char* getName() const override { return "Bloom"; }
    ShaderID getPixelShaderID() const override { return ShaderID::BloomCompositePS; }

    void setThreshold(float v) { m_params.threshold = v; }
    void setKnee(float v) { m_params.knee = v; }
    void setIntensity(float v) { m_params.intensity = v; }
    void setScatter(float v) { m_params.scatter = v; }

private:

    //! ミップレベル数（1/2 〜 1/2^MIP_COUNT）
    static constexpr int MIP_COUNT = 4;

    //! Prefilter 用定数バッファ
    struct PrefilterCBuffer
    {
        float   threshold = 1.0f;
        float   knee = 0.5f;
        Vector2 texelSize{};
    };

    //! Downsample / Upsample 共用定数バッファ
    struct BloomCBuffer
    {
        Vector2 texelSize{};
        float   scatter = 0.65f;  //!< アップサンプル時のブレンド係数
        float   padding = 0.0f;
    };

    //! 合成用定数バッファ
    struct CompositeCBuffer
    {
        float   intensity = 1.0f;
        Vector3 padding{};
    };

    //! ImGui 操作用パラメータ
    struct Params
    {
        float threshold = 1.0f;
        float knee = 0.5f;
        float intensity = 1.2f;
        float scatter = 0.65f;
    };

    Params m_params;

    //! PSO キー
    size_t m_psoPrefilter = 0;
    size_t m_psoDownsample = 0;
    size_t m_psoUpsample = 0;
    size_t m_psoComposite = 0;

    //! 中間 RT（各ミップレベル）
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_mipRT[MIP_COUNT];
    D3D12_CPU_DESCRIPTOR_HANDLE                  m_mipRTV[MIP_COUNT]{};
    UINT                                         m_mipSRV[MIP_COUNT]{};
    D3D12_RESOURCE_STATES                        m_mipState[MIP_COUNT]{};
    UINT                                         m_mipWidth[MIP_COUNT]{};
    UINT                                         m_mipHeight[MIP_COUNT]{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;

    //! 定数バッファ
    std::unique_ptr<ConstantBuffer<PrefilterCBuffer>>  m_cbPrefilter;
    std::unique_ptr<ConstantBuffer<BloomCBuffer>>      m_cbBloom;
    std::unique_ptr<ConstantBuffer<CompositeCBuffer>>  m_cbComposite;

    void createMipRenderTargets(UINT width, UINT height);
    void transitionToRT(ID3D12GraphicsCommandList* cmd, int idx);
    void transitionToSRV(ID3D12GraphicsCommandList* cmd, int idx);

    void passPrefilter(ID3D12GraphicsCommandList* cmd, UINT sceneSrvIndex);
    void passDownsample(ID3D12GraphicsCommandList* cmd, int srcIdx, int dstIdx);
    void passUpsample(ID3D12GraphicsCommandList* cmd, int srcIdx, int dstIdx);
    void passComposite(ID3D12GraphicsCommandList* cmd, UINT sceneSrvIndex, UINT bloomSrvIndex);
};