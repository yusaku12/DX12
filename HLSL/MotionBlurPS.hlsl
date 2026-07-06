#include "PostEffect.hlsli"
#include "Common.hlsli"
#include "MaterialGraphGenerated.hlsli"

//!=======================================================
//! スクリーン空間モーションブラー
//! 深度 + 前フレームVPから速度を再構築
//!=======================================================

Texture2D depthTexture : register(t1);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_currentViewProj;
    row_major float4x4 g_prevViewProj;
    row_major float4x4 g_invViewProj;
    float4 g_params0; //!< x=shutterSpeed y=maxBlurRadius z=deltaTime w=blendWeight
    float4 g_params1; //!< x=nearZ y=farZ z=texelSize.x w=texelSize.y
    float4 g_graph;   //!< x=graphId y=metallic z=roughness w=ao
    float4 g_graphBlend; //!< x=blend
};

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 center = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    float depth = depthTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).r;

    float2 ndc;
    ndc.x = input.uv.x * 2.0f - 1.0f;
    ndc.y = 1.0f - input.uv.y * 2.0f;

    float4 clip = float4(ndc, depth * 2.0f - 1.0f, 1.0f);
    float4 world = mul(g_invViewProj, clip);
    world /= max(world.w, 1e-6f);

    float4 curr = mul(g_currentViewProj, world);
    float4 prev = mul(g_prevViewProj, world);

    float2 currNdc = curr.xy / max(curr.w, 1e-6f);
    float2 prevNdc = prev.xy / max(prev.w, 1e-6f);

    float2 velocityUv = (currNdc - prevNdc) * 0.5f;
    float2 velocity = velocityUv * (g_params0.x * g_params0.z);

    float2 texelSize = g_params1.zw;
    float maxLen = g_params0.y * max(texelSize.x, texelSize.y);
    float len = length(velocity);

    if (len < 1e-5f || g_params0.w <= 0.0f)
        return float4(center, 1.0f);

    if (len > maxLen)
        velocity *= maxLen / len;

    const int SampleCount = 6;
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
    MaterialGraphResult graph = EvaluatePostEffectGraphById((int)g_graph.x, input.uv, float4(base, 1.0f), pbr, sceneTexture, depthTexture, samplerStates[LINEAR_CLAMP]);
    float blend = saturate(g_graphBlend.x);
    float3 outColor = lerp(base, graph.baseColor.rgb, blend);
    float outAlpha = lerp(1.0f, graph.alpha, blend);
    return float4(outColor, outAlpha);
}
