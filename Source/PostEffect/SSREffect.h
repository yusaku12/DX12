#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! スクリーンスペース反射 (SSR)
//!=======================================================
class SSREffect : public PostEffectBase
{
public:

    SSREffect() { m_priority = 40; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "SSR"; }
    ShaderID getPixelShaderID() const override { return ShaderID::SSRPS; }
    bool needsDepth() const override { return true; }

private:

    struct CBuffer
    {
        Matrix projection{};
        Matrix invProjection{};
        Matrix view{};
        Vector4 params0{}; //!< x=maxDistance y=thickness z=stride w=intensity
        Vector4 params1{}; //!< x=near y=far z=maxSteps w=blendWeight
        Vector4 params2{}; //!< x=fresnelBias y=fresnelPow z=roughnessCutoff w=edgeFade
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    float m_maxDistance = 30.0f;
    float m_thickness = 0.12f;
    float m_stride = 0.6f;
    float m_intensity = 0.8f;
    int m_maxSteps = 48;
    float m_fresnelBias = 0.04f;
    float m_fresnelPower = 5.0f;
    float m_roughnessCutoff = 0.7f;
    float m_edgeFade = 0.2f;
};