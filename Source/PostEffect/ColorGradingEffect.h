#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! カラーグレーディングエフェクト
//! 機能:
//!   - 露出補正 (EV)
//!   - 自動露出（ログ平均輝度ベース）
//!   - ホワイトバランス（色温度・ティント）
//!   - コントラスト / 彩度 / 色相シフト
//!   - Shadows / Midtones / Highlights カラーホイール
//!   - トーンマッピング（ACES 統一）
//!=======================================================
class ColorGradingEffect : public PostEffectBase
{
public:

    ColorGradingEffect() { m_priority = 100; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName()          const override { return "ColorGrading"; }
    ShaderID    getPixelShaderID() const override { return ShaderID::ColorGradingPS; }

    // パラメータセッター
    void setExposure(float ev) { m_params.exposure = ev; }
    void setAutoExposure(bool value) { m_params.autoExposureEnabled = value ? 1.0f : 0.0f; }
    void setExposureCompensation(float ev) { m_params.exposureCompensation = ev; }
    void setMiddleGray(float value) { m_params.middleGray = value; }
    void setAutoExposureMinEV(float value) { m_params.minEV = value; }
    void setAutoExposureMaxEV(float value) { m_params.maxEV = value; }
    void setAutoExposureStrength(float value) { m_params.autoExposureStrength = value; }
    void setTemperature(float v) { m_params.temperature = v; }
    void setTint(float v) { m_params.tint = v; }
    void setContrast(float v) { m_params.contrast = v; }
    void setSaturation(float v) { m_params.saturation = v; }
    void setHueShift(float deg) { m_params.hueShift = deg; }
    void setShadows(const Vector3& col) { m_params.shadows = col; }
    void setMidtones(const Vector3& col) { m_params.midtones = col; }
    void setHighlights(const Vector3& col) { m_params.highlights = col; }

private:

    //! GPU へ転送する定数バッファ構造体（HLSL と一致）
    struct CBuffer
    {
        // 露出 / ホワイトバランス
        float   exposure = 0.0f;
        float   autoExposureEnabled = 1.0f;
        float   exposureCompensation = 0.0f;
        float   middleGray = 0.18f;

        float   minEV = -6.0f;
        float   maxEV = 6.0f;
        float   autoExposureStrength = 1.0f;
        float   padAuto = 0.0f;

        float   temperature = 0.0f;
        float   tint = 0.0f;
        float   pad0[2] = {};

        // カラー補正
        float   contrast = 1.0f;
        float   saturation = 1.0f;
        float   hueShift = 0.0f;
        float   pad1 = 0.0f;

        // カラーホイール
        Vector3 shadows = { 0.0f, 0.0f, 0.0f };
        float   shadowsBalance = 0.5f;
        Vector3 midtones = { 0.0f, 0.0f, 0.0f };
        float   midtonesBalance = 0.5f;
        Vector3 highlights = { 0.0f, 0.0f, 0.0f };
        float   highlightsBalance = 0.5f;
    };

    CBuffer m_params;
    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;
};