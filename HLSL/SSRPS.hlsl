#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    float4 g_params0; //!< x=maxDistance y=thickness z=stride w=intensity
    float4 g_params1; //!< x=near y=far z=maxSteps w=blendWeight
    float4 g_params2; //!< x=fresnelBias y=fresnelPow z=roughnessCutoff w=edgeFade
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

float edgeAttenuation(float2 uv, float edgeFade)
{
    float2 d = min(uv, 1.0f - uv);
    float m = min(d.x, d.y);
    return saturate(m / max(edgeFade, 1e-4f));
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 scene = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;

    if (g_params1.w <= 0.0f)
    {
        return float4(scene, 1.0f);
    }

    float depth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).r;
    if (depth >= 0.99999f)
    {
        return float4(scene, 1.0f);
    }

    float roughness = DecodeRoughness(input.uv);
    if (roughness > g_params2.z)
    {
        return float4(scene, 1.0f);
    }

    float3 viewPos = ReconstructViewPosition(input.uv, depth);
    float3 normal = DecodeNormal(input.uv);
    float3 viewDir = normalize(-viewPos);
    float3 reflDir = normalize(reflect(-viewDir, normal));

    float maxDistance = g_params0.x;
    float stride = g_params0.z;
    int maxSteps = clamp((int)round(g_params1.z), 8, 128);

    bool hit = false;
    float2 hitUv = input.uv;

    [loop]
    for (int i = 1; i <= 128; ++i)
    {
        if (i > maxSteps)
        {
            break;
        }

        float t = (i / (float)maxSteps) * maxDistance;
        float3 sampleViewPos = viewPos + reflDir * t * stride;

        float sampleViewDepth = -sampleViewPos.z;
        if (sampleViewDepth <= 0.0f)
        {
            continue;
        }

        float2 uv = ProjectViewToUv(sampleViewPos);
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
        float depthDiff = sampleViewDepth - sceneDepth;

        if (abs(depthDiff) <= g_params0.y)
        {
            hit = true;
            hitUv = uv;
            break;
        }
    }

    if (!hit)
    {
        return float4(scene, 1.0f);
    }

    float3 reflection = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], hitUv, 0).rgb;

    float ndotv = saturate(dot(normal, viewDir));
    float fresnel = g_params2.x + (1.0f - g_params2.x) * pow(1.0f - ndotv, max(g_params2.y, 0.5f));
    float roughAtten = 1.0f - saturate(roughness / max(g_params2.z, 1e-4f));
    float edge = edgeAttenuation(hitUv, g_params2.w);

    float reflectWeight = saturate(g_params0.w) * saturate(g_params1.w) * fresnel * roughAtten * edge;
    float3 outColor = lerp(scene, reflection, reflectWeight);

    return float4(outColor, 1.0f);
}