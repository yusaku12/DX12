#include "PostEffect.hlsli"
#include "Common.hlsli"

// 元シーン + ブルームテクスチャの加算合成

cbuffer CBuffer : register(b0)
{
    float  g_intensity;     //!< ブルーム強度
    float3 g_padding;
};

Texture2D    g_bloomTexture : register(t1);     //!< ブルームテクスチャ

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float3 scene = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    float3 bloom = g_bloomTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;

    //! 加算合成
    float3 result = scene + bloom * g_intensity;

    return float4(result, 1.0);
}
