#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! Volumetric Fog + Atmosphere
//!=======================================================
class VolumetricFogEffect : public PostEffectBase
{
public:

    VolumetricFogEffect() { m_priority = 60; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "Volumetric Fog"; }
    ShaderID getPixelShaderID() const override { return ShaderID::VolumetricFogPS; }
    bool needsDepth() const override { return true; }

private:

    struct CBuffer
    {
        Matrix projection{};
        Matrix invProjection{};
        Matrix invView{};
        Vector4 cameraPos{};          //!< xyz=cameraPos
        Vector4 cameraNearFar{};      //!< x=near y=far
        Vector4 lightDir{};           //!< xyz=main light dir (world)
        Vector4 fogColor{};           //!< rgb=fog tint
        Vector4 params0{};            //!< x=density y=heightFalloff z=maxDistance w=stepCount
        Vector4 params1{};            //!< x=anisotropy y=inscatter y=ambient z=atmoStrength w=blendWeight
        Vector4 params2{};            //!< x=groundHeight y=horizonBoost z=depthFogBias w=reserved
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    Vector3 m_fogColor = Vector3(0.62f, 0.70f, 0.78f);
    float m_density = 0.035f;
    float m_heightFalloff = 0.045f;
    float m_maxDistance = 120.0f;
    int m_stepCount = 48;
    float m_anisotropy = 0.3f;
    float m_inscatter = 1.0f;
    float m_ambient = 0.35f;
    float m_atmoStrength = 0.45f;
    float m_groundHeight = 0.0f;
    float m_horizonBoost = 0.8f;
    float m_depthFogBias = 0.1f;
};
