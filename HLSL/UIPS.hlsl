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

float median3(float a, float b, float c)
{
    return max(min(a, b), min(max(a, b), c));
}

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PS(PSInput input) : SV_TARGET
{
    float4 baseColor;

    if (g_textureMode == 0u)
    {
        //! ソリッドカラー（テクスチャなし）
        baseColor = input.color;
    }
    else if (g_textureMode == 2u)
    {
        //! 旧フォント互換: R8 テクスチャの赤チャンネルをアルファマスクとして使用
        float alpha = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord).r;
        baseColor = float4(input.color.rgb, input.color.a * alpha);
    }
    else if (g_textureMode == 3u)
    {
        //! MSDF フォント
        float3 msd = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord).rgb;
        float sd = median3(msd.r, msd.g, msd.b);
        float width = max(fwidth(sd), 1.0/128.0);
        float alpha = smoothstep(0.5 - width, 0.5 + width, sd);
        baseColor = float4(input.color.rgb, input.color.a * alpha);
    }
    else
    {
        //! RGBA テクスチャ（モード 1）
        float4 texColor = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord);
        baseColor = texColor * input.color;
    }

    return baseColor;
}
