#pragma once

#include "PostEffectBase.h"

//=====================================================
//! グレースケールエフェクト
//=====================================================
class GrayScaleEffect : public PostEffectBase
{
public:

    GrayScaleEffect() { m_priority = 100; }

    void initialize() override;
    void render(ID3D12GraphicsCommandList* cmd, UINT inputSrvIndex) override;
    void inspectGUI() override;
    const char* getName() const override { return "GrayScale"; }
    ShaderID getPixelShaderID() const override { return ShaderID::GrayScalePS; }

private:

    //! 定数バッファ構造体
    struct CBuffer
    {
        float strength = 1.0f;
        float padding[3]{};
    };

    CBuffer m_params;
    std::unique_ptr<ConstantBuffer<CBuffer>> m_constantBuffer;
};