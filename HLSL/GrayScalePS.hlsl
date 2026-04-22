#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D inputTexture : register(t0);

cbuffer cbPostEffect : register(b0)
{
    float strength;
    float3 _padding;
};

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float4 color = inputTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv);
    float luminance = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(color.rgb, float3(luminance, luminance, luminance), strength);
    return color;
}
