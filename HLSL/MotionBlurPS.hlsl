#include "PostEffect.hlsli"
#include "Common.hlsli"
#include "MaterialGraphGenerated.hlsli"

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
    float4 g_params0; //!< x=shutterSpeed y=maxBlurRadius z=deltaTime w=blendWeight
    float4 g_params1; //!< x=texelSize.x y=texelSize.y
    float4 g_graph;   //!< x=graphId y=metallic z=roughness w=ao
    float4 g_graphBlend; //!< x=blend
};

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 center = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    const float kVelocityEncodeScale = 8.0f;
    float2 velocityUv = velocityTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).xy;
    velocityUv = (velocityUv - 0.5f) / kVelocityEncodeScale;
    float2 velocity = velocityUv * (g_params0.x * g_params0.z);

    float2 texelSize = g_params1.xy;
    float maxLen = g_params0.y * max(texelSize.x, texelSize.y);
    float len = length(velocity);

    if (len < 1e-5f || g_params0.w <= 0.0f)
        return float4(center, 1.0f);

    if (len > maxLen)
        velocity *= maxLen / len;

    const int SampleCount = 3;
    float3 accum = center;
    float total = 1.0f;

    [unroll]
    for (int i = 1; i <= SampleCount; ++i)
    {
        float t = i / (float)SampleCount;
        float2 offset = velocity * t;

        float3 c0 = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv + offset).rgb;
        float3 c1 = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv - offset).rgb;

        accum += (c0 + c1);
        total += 2.0f;
    }

    float3 blurred = accum / max(total, 1e-4f);
    float blurAmount = saturate(len / maxLen) * g_params0.w;

    float3 base = lerp(center, blurred, blurAmount);
    float3 pbr = float3(g_graph.y, g_graph.z, g_graph.w);
    MaterialGraphResult graph = EvaluatePostEffectGraphById((int)g_graph.x, input.uv, float4(base, 1.0f), pbr, sceneTexture, sceneTexture, samplerStates[LINEAR_CLAMP]);
    float blend = saturate(g_graphBlend.x);
    float3 outColor = lerp(base, graph.baseColor.rgb, blend);
    float outAlpha = lerp(1.0f, graph.alpha, blend);
    return float4(outColor, outAlpha);
}
