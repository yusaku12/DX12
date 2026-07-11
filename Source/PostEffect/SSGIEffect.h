#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! スクリーンスペース GI
//!=======================================================
class SSGIEffect : public PostEffectBase
{
public:

    SSGIEffect() { m_priority = 36; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "SSGI"; }
    ShaderID getPixelShaderID() const override { return ShaderID::SSGIPS; }
    bool needsDepth() const override { return true; }

private:

    struct CBuffer
    {
        Matrix projection{};
        Matrix invProjection{};
        Vector4 params0{}; //!< x=intensity y=maxDistance z=thickness w=stepScale
        Vector4 params1{}; //!< x=near y=far z=maxSteps w=samples
        Vector4 params2{}; //!< x=blendWeight y=normalWeight z=saturation w=maxRadiance
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    float m_intensity = 0.65f;
    float m_maxDistance = 8.0f;
    float m_thickness = 0.2f;
    float m_stepScale = 1.0f;
    int m_maxSteps = 20;
    int m_samples = 8;
    float m_normalWeight = 0.6f;
    float m_saturation = 0.95f;
    float m_maxRadiance = 1.25f;
};
