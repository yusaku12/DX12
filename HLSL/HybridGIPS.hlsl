#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);
TextureCube irradianceTex : register(t3);
Texture2D rtGiTexture : register(t5);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    row_major float4x4 g_view;
    row_major float4x4 g_invView;
    float4 g_cameraNearFar; //!< x=near y=far
    float4 g_params0;       //!< x=intensity y=ssgiWeight z=probeWeight w=maxDistance
    float4 g_params1;       //!< x=thickness y=stepStride z=maxSteps w=hemisphereSamples
    float4 g_params2;       //!< x=normalBias y=stableJitter z=blendWeight w=reserved
};

float LinearizeDepth(float depth)
{
    const float nearZ = g_cameraNearFar.x;
    const float farZ = g_cameraNearFar.y;
    return (nearZ * farZ) / max(farZ - depth * (farZ - nearZ), 1e-6f);
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

float3 DecodeNormal(float2 uv)
{
    float3 n = normalTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).xyz * 2.0f - 1.0f;
    return normalize(n);
}

float3 buildTangent(float3 n)
{
    float3 up = abs(n.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    return normalize(cross(up, n));
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 scene = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;

    if (g_params2.z <= 0.0f)
    {
        return float4(scene, 1.0f);
    }

    float depth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).r;
    if (depth >= 0.99999f)
    {
        return float4(scene, 1.0f);
    }

    float3 normalW = DecodeNormal(input.uv);
    float3 normalV = normalize(mul(float4(normalW, 0.0f), g_view).xyz);

    float3 viewPos = ReconstructViewPosition(input.uv, depth);
    float3 tangent = buildTangent(normalV);
    float3 bitangent = normalize(cross(normalV, tangent));

    int maxSteps = clamp((int)round(g_params1.z), 4, 64);
    int hemiSamples = clamp((int)round(g_params1.w), 1, 16);
    float maxDistance = g_params0.w;
    float thickness = g_params1.x;
    float stepStride = g_params1.y;
    float normalBias = g_params2.x;

    float3 ssgiAccum = 0.0f;
    float hitCount = 0.0f;

    [loop]
    for (int s = 0; s < 16; ++s)
    {
        if (s >= hemiSamples)
        {
            break;
        }

        float phi = 6.2831853f * frac((s + 0.5f) * 0.61803398875f + g_params2.y * 0.25f);
        float cosTheta = sqrt(saturate(1.0f - ((s + 0.5f) / max((float)hemiSamples, 1.0f))));
        float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));

        float3 hemi = normalize(tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normalV * cosTheta);
        float3 rayOrigin = viewPos + normalV * max(normalBias, thickness * 0.5f);

        [loop]
        for (int i = 1; i <= 64; ++i)
        {
            if (i > maxSteps)
            {
                break;
            }

            float t = (i / (float)maxSteps) * maxDistance;
            float3 sampleView = rayOrigin + hemi * t * stepStride;
            float sampleViewDepth = sampleView.z;
            if (sampleViewDepth <= 0.0f)
            {
                continue;
            }

            float2 uv = ProjectViewToUv(sampleView);
            if (any(uv <= 0.0f) || any(uv >= 1.0f))
            {
                break;
            }

            float sampleDepth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).r;
            if (sampleDepth >= 0.99999f)
            {
                continue;
            }

            float sceneDepth = LinearizeDepth(sampleDepth);
            float diff = sampleViewDepth - sceneDepth;

            if (abs(diff) <= thickness)
            {
                ssgiAccum += sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).rgb;
                hitCount += 1.0f;
                break;
            }
        }
    }

    float3 ssgi = hitCount > 0.0f ? (ssgiAccum / hitCount) : 0.0f;
    float hitRatio = saturate(hitCount / max((float)hemiSamples, 1.0f));

    float3 probeDiffuse = 0.0f;
    if (g_params0.z > 0.0001f)
    {
        probeDiffuse = irradianceTex.SampleLevel(samplerStates[LINEAR_CLAMP], normalW, 0).rgb;
    }

    float ssgiWeight = saturate(g_params0.y) * hitRatio;
    float probeWeight = saturate(g_params0.z);
    float3 indirect = ssgi * ssgiWeight + probeDiffuse * probeWeight;

    float3 rtGi = rtGiTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;
    float rtWeight = saturate(g_params2.z) * 0.35f;

    float3 outColor = scene + (indirect + rtGi * rtWeight) * g_params0.x * saturate(g_params2.z);
    return float4(outColor, 1.0f);
}
