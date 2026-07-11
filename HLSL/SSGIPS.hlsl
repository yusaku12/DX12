#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    float4 g_params0; //!< x=intensity y=maxDistance z=thickness w=stepScale
    float4 g_params1; //!< x=near y=far z=maxSteps w=samples
    float4 g_params2; //!< x=blendWeight y=normalWeight z=saturation w=maxRadiance
};

float LinearizeDepth(float depth)
{
    const float nearZ = g_params1.x;
    const float farZ = g_params1.y;
    return (nearZ * farZ) / max(farZ - depth * (farZ - nearZ), 1e-6f);
}

float3 DecodeNormal(float2 uv)
{
    float3 n = normalTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).xyz * 2.0f - 1.0f;
    return normalize(n);
}

float DecodeRoughness(float2 uv)
{
    return saturate(normalTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).w);
}

float3 ReconstructViewPosition(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 view = mul(clip, g_invProjection);
    return view.xyz / max(view.w, 1e-6f);
}

float2 ProjectViewToUv(float3 viewPos)
{
    float4 clip = mul(float4(viewPos, 1.0f), g_projection);
    float2 ndc = clip.xy / max(clip.w, 1e-6f);
    return float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float3 BuildTangent(float3 n)
{
    float3 up = abs(n.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    return normalize(cross(up, n));
}

float3 ApplySaturation(float3 color, float saturation)
{
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    return lerp(luma.xxx, color, saturation);
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 scene = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;

    if (g_params2.x <= 0.0f)
    {
        return float4(scene, 1.0f);
    }

    float depth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).r;
    if (depth >= 0.99999f)
    {
        return float4(scene, 1.0f);
    }

    float3 normal = DecodeNormal(input.uv);
    float roughness = DecodeRoughness(input.uv);
    float3 viewPos = ReconstructViewPosition(input.uv, depth);

    float3 tangent = BuildTangent(normal);
    float3 bitangent = normalize(cross(normal, tangent));

    int sampleCount = clamp((int)round(g_params1.w), 2, 16);
    int maxSteps = clamp((int)round(g_params1.z), 6, 48);
    float maxDistance = g_params0.y;
    float thickness = g_params0.z;
    float stepScale = g_params0.w;

    float3 indirectAccum = 0.0f;
    float weightAccum = 0.0f;

    [loop]
    for (int s = 0; s < 16; ++s)
    {
        if (s >= sampleCount)
        {
            break;
        }

        float noise = Hash12(input.uv * 4096.0f + float2(s * 19.7f, s * 7.9f));
        float phi = 6.2831853f * noise;
        float cosTheta = sqrt(1.0f - ((s + 0.5f) / sampleCount));
        float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
        float3 rayDir = normalize(
            tangent * (cos(phi) * sinTheta) +
            bitangent * (sin(phi) * sinTheta) +
            normal * cosTheta);

        float3 rayOrigin = viewPos + normal * 0.03f;

        [loop]
        for (int i = 1; i <= 48; ++i)
        {
            if (i > maxSteps)
            {
                break;
            }

            float t = (i / (float)maxSteps) * maxDistance * stepScale;
            float3 samplePos = rayOrigin + rayDir * t;

            if (samplePos.z <= 0.0f)
            {
                continue;
            }

            float2 sampleUv = ProjectViewToUv(samplePos);
            if (any(sampleUv <= 0.0f) || any(sampleUv >= 1.0f))
            {
                break;
            }

            float sampleDepthRaw = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], sampleUv, 0).r;
            if (sampleDepthRaw >= 0.99999f)
            {
                continue;
            }

            float sampleDepthLin = LinearizeDepth(sampleDepthRaw);
            float dz = samplePos.z - sampleDepthLin;
            if (abs(dz) > thickness)
            {
                continue;
            }

            float3 sampleNormal = DecodeNormal(sampleUv);
            float nWeight = lerp(1.0f, saturate(dot(normal, sampleNormal)), saturate(g_params2.y));
            float dWeight = 1.0f - saturate(t / maxDistance);

            float3 bounced = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], sampleUv, 0).rgb;
            float3 radiance = bounced * nWeight * dWeight;
            indirectAccum += radiance;
            weightAccum += nWeight * dWeight;
            break;
        }
    }

    float3 indirect = (weightAccum > 0.0f) ? (indirectAccum / weightAccum) : 0.0f;
    indirect = ApplySaturation(indirect, g_params2.z);

    // 鏡面寄りの面はスクリーンスペース GI の寄与を弱める
    float roughWeight = saturate(1.0f - roughness * 0.75f);
    indirect *= roughWeight;

    indirect = min(indirect * g_params0.x, g_params2.w.xxx);

    float3 outColor = scene + indirect * saturate(g_params2.x);
    return float4(outColor, 1.0f);
}
