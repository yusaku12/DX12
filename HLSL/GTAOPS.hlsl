#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_invProjection;
    float4 g_params0; //!< x=radius y=thickness z=intensity w=power
    float4 g_params1; //!< x=texelX y=texelY z=near w=far
    float4 g_params2; //!< x=stepCount y=dirCount z=blendWeight w=normalWeight
};

float LinearizeDepth(float depth)
{
    const float nearZ = g_params1.z;
    const float farZ = g_params1.w;
    return (nearZ * farZ) / max(farZ - depth * (farZ - nearZ), 1e-6f);
}

float3 DecodeNormal(float2 uv)
{
    float3 n = normalTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).xyz * 2.0f - 1.0f;
    return normalize(n);
}

float3 ReconstructViewPosition(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 view = mul(clip, g_invProjection);
    return view.xyz / max(view.w, 1e-6f);
}

float noise2D(float2 uv)
{
    float n = dot(uv, float2(12.9898f, 78.233f));
    return frac(sin(n) * 43758.5453f);
}

float3 BuildTangent(float3 n)
{
    float3 up = abs(n.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    return normalize(cross(up, n));
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 scene = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;
    float centerDepth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).r;

    if (centerDepth >= 0.99999f || g_params2.z <= 0.0f)
    {
        return float4(scene, 1.0f);
    }

    float3 normal = DecodeNormal(input.uv);
    float3 tangent = BuildTangent(normal);
    float3 bitangent = normalize(cross(normal, tangent));
    float centerLinearDepth = LinearizeDepth(centerDepth);
    float3 centerViewPos = ReconstructViewPosition(input.uv, centerDepth);
    float radiusWorld = g_params0.x;
    float radiusUv = radiusWorld / max(centerLinearDepth, 1e-4f);
    float radiusUvMax = max(g_params1.x, g_params1.y) * 96.0f;
    radiusUv = min(radiusUv, radiusUvMax);
    float thickness = g_params0.y;
    float selfBias = max(0.01f, thickness * 0.25f);

    int stepCount = clamp((int)round(g_params2.x), 2, 12);
    int dirCount = clamp((int)round(g_params2.y), 4, 16);

    float occlusion = 0.0f;
    float weightSum = 0.0f;

    [loop]
    for (int d = 0; d < 16; ++d)
    {
        if (d >= dirCount)
        {
            break;
        }

        float angle = 6.2831853f * (d + 0.5f) / dirCount;
        float2 dir2 = float2(cos(angle), sin(angle));
        float3 sampleHemisphereDir = normalize(tangent * dir2.x + bitangent * dir2.y + normal * 0.35f);

        [loop]
        for (int s = 1; s <= 12; ++s)
        {
            if (s > stepCount)
            {
                break;
            }

            float t = s / (float)stepCount;
            float2 uv = input.uv + dir2 * radiusUv * t;
            if (any(uv <= 0.0f) || any(uv >= 1.0f))
            {
                continue;
            }

            float sampleDepth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).r;
            if (sampleDepth >= 0.99999f)
            {
                continue;
            }

            float sampleLinear = LinearizeDepth(sampleDepth);
            float3 sampleViewPos = ReconstructViewPosition(uv, sampleDepth);
            float3 sampleVec = sampleViewPos - centerViewPos;
            float dist = length(sampleVec);
            if (dist <= 1.0e-4f || dist > radiusWorld)
            {
                continue;
            }

            float3 sampleDir = sampleVec / dist;
            float horizon = saturate(dot(sampleHemisphereDir, sampleDir) - selfBias);
            float rangeWeight = 1.0f - saturate(dist / radiusWorld);
            float depthWeight = saturate(1.0f - abs(sampleLinear - centerLinearDepth) / max(thickness, 1.0e-4f));

            if (horizon <= 0.0f || depthWeight <= 0.0f)
            {
                continue;
            }

            float3 sampleNormal = DecodeNormal(uv);
            float normalTerm = saturate(dot(normal, sampleNormal));
            float normalWeight = lerp(1.0f, normalTerm, saturate(g_params2.w));

            float w = rangeWeight * depthWeight * normalWeight;
            occlusion += horizon * w;
            weightSum += w;
        }
    }

    float ao = 1.0f - saturate(occlusion / max(weightSum, 1e-4f));
    ao = pow(saturate(ao), max(g_params0.w, 0.25f));
    float aoTerm = lerp(1.0f, ao, saturate(g_params0.z));

    float3 outColor = scene * aoTerm;
    outColor = lerp(scene, outColor, saturate(g_params2.z));
    return float4(outColor, 1.0f);
}