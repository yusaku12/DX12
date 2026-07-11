#pragma once

#include "PostEffectBase.h"

//=====================================================
//! モーションブラーエフェクト
//! Unity / Unreal 方式のスクリーン空間モーションブラー
//=====================================================
class MotionBlurEffect : public PostEffectBase
{
public:

    MotionBlurEffect() { m_priority = 96; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "MotionBlur"; }
    ShaderID getPixelShaderID() const override { return ShaderID::MotionBlurPS; }
    bool needsDepth() const override { return false; }

    void setShutterSpeed(float v) { m_shutterSpeed = std::max(v, 0.0f); }
    void setMaxBlurRadius(float v) { m_maxBlurRadius = std::max(v, 0.0f); }

private:

    struct CBuffer
    {
        Matrix  currentViewProj{};
        Matrix  prevViewProj{};
        Matrix  invViewProj{};
        Vector4 params0{}; //!< x=shutterScale y=maxBlurRadiusPx z=blendWeight w=velocityReject
        Vector4 params1{}; //!< x=texelSize.x y=texelSize.y z=minSamples w=maxSamples
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;
    Matrix m_prevViewProj = Matrix::Identity;
    bool m_hasPrev = false;

    float m_shutterSpeed = 1.0f;
    float m_maxBlurRadius = 20.0f;
    float m_velocityReject = 0.75f;
    int m_minSamples = 4;
    int m_maxSamples = 12;
};