#pragma once

#include "PostEffectBase.h"

//=====================================================
//! モーションブラーエフェクト
//! Unity / Unreal 方式のスクリーン空間モーションブラー
//=====================================================
class MotionBlurEffect : public PostEffectBase
{
public:

    MotionBlurEffect() { m_priority = 80; }

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
        Vector4 params0{}; //!< x=shutterSpeed y=maxBlurRadius z=deltaTime w=blendWeight
        Vector4 params1{}; //!< x=texelSize.x y=texelSize.y
        Vector4 graph{};   //!< x=graphId y=metallic z=roughness w=ao
        Vector4 graphBlend{}; //!< x=blend
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;
    Matrix m_prevViewProj = Matrix::Identity;
    bool m_hasPrev = false;

    float m_shutterSpeed = 0.5f;
    float m_maxBlurRadius = 8.0f;
    float m_graphId = 0.0f;
    float m_graphMetallic = 0.0f;
    float m_graphRoughness = 1.0f;
    float m_graphAo = 1.0f;
    float m_graphBlend = 0.0f;
};