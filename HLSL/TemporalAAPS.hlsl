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
    float4 g_texelParams;   //!< x=1/width y=1/height z=jitterX w=jitterY
    float4 g_prevJitter;    //!< x=prevJitterX y=prevJitterY
};

float2 reconstructVelocityUv(float2 uv)
{
    const float kVelocityEncodeScale = 8.0f;
    float2 velocityUv = velocityTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).xy;
    velocityUv = (velocityUv - 0.5f) / kVelocityEncodeScale;

    // ジッター差分を除去してモーションベクトルのノイズを抑える
    velocityUv -= (g_texelParams.zw - g_prevJitter.xy);

    return velocityUv;
}

float3 neighborhoodMin(float2 uv)
{
    const float2 texel = g_texelParams.xy;
    float3 mn = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).rgb;
    mn = min(mn, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(texel.x, 0.0f), 0).rgb);
    mn = min(mn, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(-texel.x, 0.0f), 0).rgb);
    mn = min(mn, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(0.0f, texel.y), 0).rgb);
    mn = min(mn, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(0.0f, -texel.y), 0).rgb);

    return mn;
}

float3 neighborhoodMax(float2 uv)
{
    const float2 texel = g_texelParams.xy;
    float3 mx = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).rgb;
    mx = max(mx, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(texel.x, 0.0f), 0).rgb);
    mx = max(mx, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(-texel.x, 0.0f), 0).rgb);
    mx = max(mx, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(0.0f, texel.y), 0).rgb);
    mx = max(mx, sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv + float2(0.0f, -texel.y), 0).rgb);

    return mx;
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    const float2 jitterUv = input.uv + g_texelParams.zw;
    const float3 current = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], jitterUv).rgb;

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

    // 近傍クランプでゴーストを抑制
    const float3 mn = neighborhoodMin(input.uv);
    const float3 mx = neighborhoodMax(input.uv);
    history = clamp(history, mn, mx);

    const float speed = length(velocityUv / max(g_texelParams.xy, float2(1e-6f, 1e-6f)));
    const float motionFactor = saturate(speed / g_blendParams.z);
    const float historyWeight = lerp(g_blendParams.x, g_blendParams.y, motionFactor);

    const float3 outColor = lerp(current, history, saturate(historyWeight));
    return float4(outColor, 1.0f);
}
