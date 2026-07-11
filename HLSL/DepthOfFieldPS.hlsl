#include "PostEffect.hlsli"
#include "Common.hlsli"

//!=======================================================
//! 被写界深度ピクセルシェーダー
//! Unity / Unreal 風の深度ベース DoF
//!=======================================================

Texture2D depthTexture : register(t1);

cbuffer CBuffer : register(b0)
{
    float  g_focusDistance;
    float  g_focusRange;
    float  g_aperture;
    float  g_maxBlurRadius;
    float2 g_texelSize;
    float  g_nearZ;
    float  g_farZ;
    float  g_blendWeight;
    float3 g_padding;
};

static const int SampleCount = 12;
static const float2 kPoisson[SampleCount] =
{
    float2(-0.326f, -0.406f),
    float2(-0.840f, -0.074f),
    float2(-0.696f,  0.457f),
    float2(-0.203f,  0.621f),
    float2(0.962f, -0.195f),
    float2(0.473f, -0.480f),
    float2(0.519f,  0.767f),
    float2(0.185f, -0.893f),
    float2(0.507f,  0.064f),
    float2(0.896f,  0.412f),
    float2(-0.322f,  0.933f),
    float2(-0.792f, -0.597f)
};

float LinearizeDepth(float depth)
{
    return (g_nearZ * g_farZ) / max(g_farZ - depth * (g_farZ - g_nearZ), 1e-6f);
}

float ComputeCoC(float linearDepth)
{
    float coc = abs(linearDepth - g_focusDistance) / max(g_focusRange, 1e-4f);
    return saturate(coc);
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 center = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    float depth = depthTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).r;
    float linearDepth = LinearizeDepth(depth);

    float coc = ComputeCoC(linearDepth);
    float radius = coc * g_maxBlurRadius * g_aperture;

    if (radius < 0.25f || g_blendWeight <= 0.0f)
        return float4(center, 1.0f);

    float3 accum = center;
    float total = 1.0f;

    [unroll]
    for (int i = 0; i < SampleCount; ++i)
    {
        float2 uv = input.uv + kPoisson[i] * radius * g_texelSize;
        float sampleDepth = depthTexture.Sample(samplerStates[LINEAR_CLAMP], uv).r;
        float sampleLinear = LinearizeDepth(sampleDepth);
        float sampleCoc = ComputeCoC(sampleLinear);

        float depthWeight = saturate(1.0f - abs(sampleLinear - linearDepth) / max(g_focusRange, 1e-3f));
        float weight = max(sampleCoc, 0.0f) * depthWeight;

        float3 sampleColor = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv).rgb;
        accum += sampleColor * weight;
        total += weight;
    }

    float3 blurred = accum / max(total, 1e-4f);
    float blend = saturate(coc * g_blendWeight);
    float3 finalColor = lerp(center, blurred, blend);
    return float4(finalColor, 1.0f);
}
