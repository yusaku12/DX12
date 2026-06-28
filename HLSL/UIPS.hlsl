//!< UI ピクセルシェーダー
//!< テクスチャモード（カラー / RGBA / フォント alpha-only）に対応

#include "Common.hlsli"

cbuffer UIConstantsCB : register(b0)
{
    row_major float4x4 g_transform;
    row_major float4x4 g_localTransform;
    float4             g_tintColor;
    uint               g_textureMode;
    float              g_globalAlpha;
    float2             g_pad;
};

Texture2D g_uiTexture : register(t0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PS(PSInput input) : SV_TARGET
{
    if (g_textureMode == 0u)
    {
        //! ソリッドカラー（テクスチャなし）
        return input.color;
    }

    if (g_textureMode == 2u)
    {
        //! フォント：R8 テクスチャの赤チャンネルをアルファマスクとして使用
        float alpha = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord).r;
        return float4(input.color.rgb, input.color.a * alpha);
    }

    //! RGBA テクスチャ（モード 1）
    float4 texColor = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord);
    return texColor * input.color;
}
