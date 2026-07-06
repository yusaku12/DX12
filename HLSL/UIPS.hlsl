//!< UI ピクセルシェーダー
//!< テクスチャモード（カラー / RGBA / フォント alpha-only）に対応

#include "Common.hlsli"
#include "MaterialGraphGenerated.hlsli"

cbuffer UIConstantsCB : register(b0)
{
    row_major float4x4 g_transform;
    row_major float4x4 g_localTransform;
    float4             g_tintColor;
    uint               g_textureMode;
    float              g_globalAlpha;
    float              g_graphId;
    float              g_graphMetallic;
    float              g_graphRoughness;
    float              g_graphAo;
    float              g_graphBlend;
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
    float4 baseColor;

    if (g_textureMode == 0u)
    {
        //! ソリッドカラー（テクスチャなし）
        baseColor = input.color;
    }
    else if (g_textureMode == 2u)
    {
        //! フォント：R8 テクスチャの赤チャンネルをアルファマスクとして使用
        float alpha = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord).r;
        baseColor = float4(input.color.rgb, input.color.a * alpha);
    }
    else
    {
        //! RGBA テクスチャ（モード 1）
        float4 texColor = g_uiTexture.Sample(samplerStates[LINEAR_CLAMP], input.texcoord);
        baseColor = texColor * input.color;
    }

    float3 pbr = float3(g_graphMetallic, g_graphRoughness, g_graphAo);
    MaterialGraphResult graph = EvaluateParticleGraphById((int)g_graphId, input.texcoord, baseColor, pbr, g_uiTexture, g_uiTexture, samplerStates[LINEAR_CLAMP]);
    float blend = saturate(g_graphBlend);
    float3 outColor = lerp(baseColor.rgb, graph.baseColor.rgb, blend);
    float outAlpha = lerp(baseColor.a, graph.alpha, blend);
    return float4(outColor, outAlpha);
}
