#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D historyTexture : register(t1);
Texture2D velocityTexture : register(t2);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_currentViewProj;
    row_major float4x4 g_prevViewProj;
    row_major float4x4 g_invViewProj;
    float4 g_blendParams;   //!< x=stationaryBlend y=motionBlend z=motionScale w=historyValid
    float4 g_texelParams;   //!< x=1/width y=1/height z=reserved w=reserved
    float4 g_prevJitter;    //!< reserved
};

float2 reconstructVelocityUv(float2 uv)
{
    return velocityTexture.SampleLevel(samplerStates[POINT_CLAMP], uv, 0).xy;
}

void neighborhoodStats(float2 uv, out float3 mn, out float3 mx, out float3 mean, out float3 sigma)
{
    const float2 texel = g_texelParams.xy;
    float3 sum = 0.0f;
    float3 sumSq = 0.0f;

    mn = float3(65504.0f, 65504.0f, 65504.0f);
    mx = float3(-65504.0f, -65504.0f, -65504.0f);

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y) * texel;
            float3 c = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + offset, 0).rgb;
            mn = min(mn, c);
            mx = max(mx, c);
            sum += c;
            sumSq += c * c;
        }
    }

    const float invCount = 1.0f / 9.0f;
    mean = sum * invCount;
    float3 variance = max(sumSq * invCount - mean * mean, 0.0f);
    sigma = sqrt(variance);
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    const float3 current = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;

    if (g_blendParams.w <= 0.5f)
    {
        return float4(current, 1.0f);
    }

    const float2 velocityUv = reconstructVelocityUv(input.uv);
    float2 historyUv = input.uv - velocityUv;

    const bool historyInBounds = all(historyUv >= float2(0.0f, 0.0f)) && all(historyUv <= float2(1.0f, 1.0f));
    if (!historyInBounds)
    {
        return float4(current, 1.0f);
    }

    float3 history = historyTexture.SampleLevel(samplerStates[LINEAR_CLAMP], historyUv, 0).rgb;

    // 近傍分布を使ったクリップでゴーストを抑制
    float3 mn, mx, mean, sigma;
    neighborhoodStats(input.uv, mn, mx, mean, sigma);
    const float3 aabbClamped = clamp(history, mn, mx);
    const float3 sigmaClamped = clamp(aabbClamped, mean - sigma * 1.25f, mean + sigma * 1.25f);
    history = sigmaClamped;

    const float speed = length(velocityUv / max(g_texelParams.xy, float2(1e-6f, 1e-6f)));
    const float motionFactor = saturate(speed / g_blendParams.z);
    float historyWeight = lerp(g_blendParams.x, g_blendParams.y, motionFactor);

    const float3 lumaWeights = float3(0.2126f, 0.7152f, 0.0722f);
    const float currentLuma = dot(current, lumaWeights);
    const float historyLuma = dot(history, lumaWeights);
    const float lumaDelta = abs(currentLuma - historyLuma) / max(max(currentLuma, historyLuma), 1e-3f);
    const float reactive = saturate(lumaDelta * 2.5f);
    historyWeight *= (1.0f - reactive);

    const float3 outColor = lerp(current, history, saturate(min(historyWeight, 0.97f)));
    return float4(outColor, 1.0f);
}
