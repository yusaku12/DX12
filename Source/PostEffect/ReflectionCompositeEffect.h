#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! SSR + Reflection Probe 合成
//!=======================================================
class ReflectionCompositeEffect : public PostEffectBase
{
public:

    ReflectionCompositeEffect() { m_priority = 42; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "Reflection Composite"; }
    ShaderID getPixelShaderID() const override { return ShaderID::ReflectionCompositePS; }
    bool needsDepth() const override { return true; }

private:

    struct CBuffer
    {
        Matrix projection{};
        Matrix invProjection{};
        Matrix view{};
        Matrix invView{};
        Vector4 cameraNearFar{};      //!< x=near y=far
        Vector4 params0{};            //!< x=maxDistance y=thickness z=stride w=intensity
        Vector4 params1{};            //!< x=maxSteps y=fresnelBias z=fresnelPower w=roughnessCutoff
        Vector4 params2{};            //!< x=edgeFade y=probeStrength z=ssrStrength w=blendWeight
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    float m_maxDistance = 40.0f;
    float m_thickness = 0.12f;
    float m_stride = 0.7f;
    float m_intensity = 1.0f;
    int m_maxSteps = 64;
    float m_fresnelBias = 0.04f;
    float m_fresnelPower = 5.0f;
    float m_roughnessCutoff = 0.9f;
    float m_edgeFade = 0.2f;
    float m_probeStrength = 0.7f;
    float m_ssrStrength = 1.0f;
};
