#include "PostEffect.hlsli"
#include "Common.hlsli"

//!=======================================================
//! スクリーン空間モーションブラー
//! 速度バッファを参照するスクリーン空間モーションブラー
//!=======================================================

Texture2D velocityTexture : register(t1);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_currentViewProj;
    row_major float4x4 g_prevViewProj;
    row_major float4x4 g_invViewProj;
    float4 g_params0; //!< x=shutterScale y=maxBlurRadiusPx z=blendWeight w=velocityReject
    float4 g_params1; //!< x=texelSize.x y=texelSize.y z=minSamples w=maxSamples
};

float2 decodeVelocityUv(float2 uv)
{
    return velocityTexture.SampleLevel(samplerStates[POINT_CLAMP], uv, 0).xy;
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 center = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;
    float2 velocityUv = decodeVelocityUv(input.uv);
    float2 velocity = velocityUv * g_params0.x;

    float2 texelSize = g_params1.xy;
    float maxLen = g_params0.y * max(texelSize.x, texelSize.y);
    float len = length(velocity);

    if (len < 1e-5f || g_params0.z <= 0.0f)
        return float4(center, 1.0f);

    if (len > maxLen)
        velocity *= maxLen / len;

    const int kKernelMax = 16;
    int minSamples = clamp((int)round(g_params1.z), 2, kKernelMax);
    int maxSamples = clamp((int)round(g_params1.w), minSamples, kKernelMax);
    int sampleCount = (int)round(lerp((float)minSamples, (float)maxSamples, saturate(len / max(maxLen, 1e-6f))));

    float2 dir = velocity / max(length(velocity), 1e-6f);
    float3 accum = center;
    float total = 1.0f;

    [loop]
    for (int i = 1; i <= kKernelMax; ++i)
    {
        if (i > sampleCount)
            break;

        float t = i / (float)sampleCount;
        float w = 1.0f - t;
        float2 offset = velocity * t;

        float2 uv0 = input.uv + offset;
        float2 uv1 = input.uv - offset;

        float3 c0 = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv0, 0).rgb;
        float3 c1 = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv1, 0).rgb;

        float2 sv0 = decodeVelocityUv(uv0);
        float2 sv1 = decodeVelocityUv(uv1);
        float2 nsv0 = sv0 / max(length(sv0), 1e-6f);
        float2 nsv1 = sv1 / max(length(sv1), 1e-6f);

        float a0 = saturate(dot(dir, nsv0) * 0.5f + 0.5f);
        float a1 = saturate(dot(-dir, nsv1) * 0.5f + 0.5f);
        float velocityAgreement0 = lerp(1.0f, a0 * a0, saturate(g_params0.w));
        float velocityAgreement1 = lerp(1.0f, a1 * a1, saturate(g_params0.w));
        float w0 = w * velocityAgreement0;
        float w1 = w * velocityAgreement1;

        accum += c0 * w0;
        accum += c1 * w1;
        total += w0 + w1;
    }

    float3 blurred = accum / max(total, 1e-4f);
    float blurAmount = saturate(len / max(maxLen, 1e-6f)) * g_params0.z;
    float3 base = lerp(center, blurred, blurAmount);
    return float4(base, 1.0f);
}
