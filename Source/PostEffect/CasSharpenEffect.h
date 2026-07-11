#pragma once

#include "PostEffectBase.h"

//=====================================================
//! CAS相当の軽量シャープニング
//! TAA後段で解像感を回復する
//=====================================================
class CasSharpenEffect : public PostEffectBase
{
public:

    CasSharpenEffect() { m_priority = 110; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;

    const char* getName() const override { return "FSR RCAS"; }
    ShaderID getPixelShaderID() const override { return ShaderID::CasSharpenPS; }

private:

    struct CBuffer
    {
        Vector4 params0{}; //!< x=strength y=clampAmount z=texelX w=texelY
    };

    std::unique_ptr<ConstantBuffer<CBuffer>> m_cb;
    float m_strength = 0.35f;
    float m_clampAmount = 0.20f;
};
