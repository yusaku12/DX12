#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! カラーグレーディングエフェクト
//! 機能:
//!   - 露出補正 (EV)
//!   - ホワイトバランス（色温度・ティント）
//!   - コントラスト / 彩度 / 色相シフト
//!   - Shadows / Midtones / Highlights カラーホイール
//!   - トーンマッピング（Linear / ACES / Filmic）
//!=======================================================
class ColorGradingEffect : public PostEffectBase
{
public:

    //! トーンマッピングモード
    enum class ToneMapMode : int
    {
        Linear = 0,
        ACES = 1,
        Filmic = 2,
    };

    ColorGradingEffect() { m_priority = 100; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName()          const override { return "ColorGrading"; }
    ShaderID    getPixelShaderID() const override { return ShaderID::ColorGradingPS; }

    // パラメータセッター
    void setExposure(float ev) { m_params.exposure = ev; }
    void setTemperature(float v) { m_params.temperature = v; }
    void setTint(float v) { m_params.tint = v; }
    void setContrast(float v) { m_params.contrast = v; }
    void setSaturation(float v) { m_params.saturation = v; }
    void setHueShift(float deg) { m_params.hueShift = deg; }
    void setShadows(const Vector3& col) { m_params.shadows = col; }
    void setMidtones(const Vector3& col) { m_params.midtones = col; }
    void setHighlights(const Vector3& col) { m_params.highlights = col; }
    void setToneMapMode(ToneMapMode mode) { m_params.tonemapMode = static_cast<int>(mode); }

private:

    //! GPU へ転送する定数バッファ構造体（HLSL と一致）
    struct CBuffer
    {
        // 露出 / ホワイトバランス
        float   exposure = 0.0f;
        float   temperature = 0.0f;
        float   tint = 0.0f;
        float   pad0 = 0.0f;

        // カラー補正
        float   contrast = 1.0f;
        float   saturation = 1.0f;
        float   hueShift = 0.0f;
        float   pad1 = 0.0f;

        // カラーホイール
        Vector3 shadows = { 0.5f, 0.5f, 0.5f };
        float   shadowsBalance = 2.0f;
        Vector3 midtones = { 0.5f, 0.5f, 0.5f };
        float   midtonesBalance = 2.0f;
        Vector3 highlights = { 0.5f, 0.5f, 0.5f };
        float   highlightsBalance = 2.0f;

        // トーンマップ
        int     tonemapMode = static_cast<int>(ToneMapMode::ACES);
        float   graphId = 0.0f;
        float   graphMetallic = 0.0f;
        float   graphRoughness = 1.0f;
        float   graphAo = 1.0f;
        float   graphBlend = 1.0f;
        Vector3 pad2 = {};
    };

    CBuffer m_params;
    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;
};