#pragma once

#include "PostEffectBase.h"

//!=======================================================
//! GTAO 近似スクリーンスペース AO
//!=======================================================
class GTAOEffect : public PostEffectBase
{
public:

    GTAOEffect() { m_priority = 35; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "GTAO"; }
    ShaderID getPixelShaderID() const override { return ShaderID::GTAOPS; }
    bool needsDepth() const override { return true; }

private:

    struct CBuffer
    {
        Matrix invProjection{};
        Matrix view{};
        Vector4 params0{}; //!< x=radius y=thickness z=intensity w=power
        Vector4 params1{}; //!< x=texelX y=texelY z=near w=far
        Vector4 params2{}; //!< x=stepCount y=dirCount z=blendWeight w=normalWeight
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;

    float m_radius = 1.2f;
    float m_thickness = 0.15f;
    float m_intensity = 1.0f;
    float m_power = 1.4f;
    int m_stepCount = 5;
    int m_directionCount = 8;
    float m_normalWeight = 0.65f;
};