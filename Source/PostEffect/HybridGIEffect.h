#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! ハイブリッドGI（スクリーンスペース + プローブ）
//!=======================================================
class HybridGIEffect : public PostEffectBase
{
public:

    HybridGIEffect() { m_priority = 33; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "Hybrid GI"; }
    ShaderID getPixelShaderID() const override { return ShaderID::HybridGIPS; }
    bool needsDepth() const override { return true; }

private:

    struct CBuffer
    {
        Matrix projection{};
        Matrix invProjection{};
        Matrix view{};
        Matrix invView{};
        Vector4 cameraNearFar{};      //!< x=near y=far
        Vector4 params0{};            //!< x=intensity y=ssgiWeight z=probeWeight w=maxDistance
        Vector4 params1{};            //!< x=thickness y=stepStride z=maxSteps w=hemisphereSamples
        Vector4 params2{};            //!< x=normalBias y=temporalStableJitter z=blendWeight w=reserved
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    float m_intensity = 0.75f;
    float m_ssgiWeight = 0.65f;
    float m_probeWeight = 0.35f;
    float m_maxDistance = 12.0f;
    float m_thickness = 0.18f;
    float m_stepStride = 0.65f;
    int m_maxSteps = 24;
    int m_hemisphereSamples = 8;
    float m_normalBias = 0.04f;
    float m_temporalStableJitter = 0.35f;
};
