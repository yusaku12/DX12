#pragma once

#include "PostEffectBase.h"

//=====================================================
//! 被写界深度エフェクト
//! Unity / Unreal 風の可変ボケを深度ベースで適用
//=====================================================
class DepthOfFieldEffect : public PostEffectBase
{
public:

    DepthOfFieldEffect() { m_priority = 70; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "DepthOfField"; }
    ShaderID getPixelShaderID() const override { return ShaderID::DepthOfFieldPS; }
    bool needsDepth() const override { return true; }

    void setFocusDistance(float v) { m_params.focusDistance = std::max(v, 0.0f); }
    void setFocusRange(float v) { m_params.focusRange = std::max(v, 0.01f); }
    void setAperture(float v) { m_params.aperture = std::max(v, 0.0f); }
    void setMaxBlurRadius(float v) { m_params.maxBlurRadius = std::max(v, 0.0f); }

private:

    struct CBuffer
    {
        float   focusDistance = 5.0f;  //!< フォーカス距離
        float   focusRange = 2.0f;     //!< フォーカス幅
        float   aperture = 1.0f;       //!< 絞り
        float   maxBlurRadius = 12.0f; //!< 最大ブラー半径（ピクセル）
        Vector2 texelSize{};           //!< 1/ScreenSize
        float   nearZ = 0.1f;          //!< 近クリップ
        float   farZ = 1000.0f;        //!< 遠クリップ
        float   blendWeight = 1.0f;    //!< ボリュームブレンド
        Vector3 padding{};             //!< パディング
    };

    CBuffer m_params;
    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;
};