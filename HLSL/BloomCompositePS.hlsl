#include "PostEffect.hlsli"
#include "Common.hlsli"
#include "MaterialGraphGenerated.hlsli"

// 元シーン + ブルームテクスチャの加算合成

cbuffer CBuffer : register(b0)
{
    float  g_intensity;     //!< ブルーム強度
    float  g_graphId;
    float  g_graphMetallic;
    float  g_graphRoughness;
    float  g_graphAo;
    float  g_graphBlend;
    float3 g_padding;
};

Texture2D    g_bloomTexture : register(t1);     //!< ブルームテクスチャ

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float3 scene = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    float3 bloom = g_bloomTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;

    //! 加算合成
    float3 result = scene + bloom * g_intensity;

    float3 pbr = float3(g_graphMetallic, g_graphRoughness, g_graphAo);
    MaterialGraphResult graph = EvaluatePostEffectGraphById((int)g_graphId, input.uv, float4(result, 1.0f), pbr, sceneTexture, g_bloomTexture, samplerStates[LINEAR_CLAMP]);
    float blend = saturate(g_graphBlend);
    float3 outColor = lerp(result, graph.baseColor.rgb, blend);
    float outAlpha = lerp(1.0f, graph.alpha, blend);
    return float4(outColor, outAlpha);
}
