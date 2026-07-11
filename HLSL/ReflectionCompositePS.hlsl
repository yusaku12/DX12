#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);
TextureCube prefilterTex : register(t4);
Texture2D rtReflectionTexture : register(t5);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    row_major float4x4 g_view;
    row_major float4x4 g_invView;
    float4 g_cameraNearFar; //!< x=near y=far
    float4 g_params0;       //!< x=maxDistance y=thickness z=stride w=intensity
    float4 g_params1;       //!< x=maxSteps y=fresnelBias z=fresnelPower w=roughnessCutoff
    float4 g_params2;       //!< x=edgeFade y=probeStrength z=ssrStrength w=blendWeight
    float4 g_params3;       //!< x=rtStrength y=probeMinMix z=ssrConfidencePower w=reserved
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

float DecodeRoughness(float2 uv)
{
    return saturate(normalTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).w);
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

    if (g_params2.w <= 0.0f)
    {
        return float4(scene, 1.0f);
    }

    float depth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).r;
    if (depth >= 0.99999f)
    {
        return float4(scene, 1.0f);
    }

    float roughness = DecodeRoughness(input.uv);
    if (roughness > g_params1.w)
    {
        return float4(scene, 1.0f);
    }

    float3 normalW = DecodeNormal(input.uv);
    float3 normalV = normalize(mul(float4(normalW, 0.0f), g_view).xyz);

    float3 viewPos = ReconstructViewPosition(input.uv, depth);
    float3 viewDirV = normalize(-viewPos);
    float3 reflDirV = normalize(reflect(-viewDirV, normalV));

    float maxDistance = g_params0.x;
    float stride = g_params0.z;
    int maxSteps = clamp((int)round(g_params1.x), 8, 128);

    bool hit = false;
    float2 hitUv = input.uv;
    float hitStep = 0.0f;

    [loop]
    for (int i = 1; i <= 128; ++i)
    {
        if (i > maxSteps)
        {
            break;
        }

        float t = (i / (float)maxSteps) * maxDistance;
        float3 sampleViewPos = viewPos + reflDirV * t * stride;
        if (sampleViewPos.z <= 0.0f)
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
        float depthDiff = sampleViewPos.z - sceneDepth;

        if (abs(depthDiff) <= g_params0.y)
        {
            hit = true;
            hitUv = uv;
            hitStep = i / (float)maxSteps;
            break;
        }
    }

    float3 reflColorSSR = scene;
    float ssrConfidence = 0.0f;
    if (hit)
    {
        reflColorSSR = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], hitUv, 0).rgb;
        ssrConfidence = edgeAttenuation(hitUv, g_params2.x) * (1.0f - hitStep);
    }

    float4 worldPos4 = mul(float4(viewPos, 1.0f), g_invView);
    float3 worldPos = worldPos4.xyz / max(worldPos4.w, 1e-6f);
    float3 cameraPos = g_invView[3].xyz;
    float3 viewDirW = normalize(cameraPos - worldPos);
    float3 reflectDirW = normalize(reflect(-viewDirW, normalW));

    float3 reflColorProbe = 0.0f;
    if (g_params2.y > 0.0001f)
    {
        uint mipLevels = 1;
        uint cubeSize = 1;
        prefilterTex.GetDimensions(0, cubeSize, cubeSize, mipLevels);
        float lod = roughness * max(1.0f, (float)mipLevels - 1.0f);
        reflColorProbe = prefilterTex.SampleLevel(samplerStates[LINEAR_CLAMP], reflectDirW, lod).rgb;
    }

    float ndotv = saturate(dot(normalW, viewDirW));
    float fresnel = g_params1.y + (1.0f - g_params1.y) * pow(1.0f - ndotv, max(g_params1.z, 0.5f));

    float roughness01 = roughness / max(g_params1.w, 1e-4f);
    float roughnessFactor = saturate(roughness01);
    float confidence = pow(saturate(ssrConfidence), max(g_params3.z, 0.5f));
    float ssrMix = confidence * (1.0f - roughnessFactor);
    float probeMix = max(saturate(g_params3.y), 1.0f - ssrMix);

    float weightedProbe = probeMix * saturate(g_params2.y);
    float weightedSSR = ssrMix * saturate(g_params2.z);
    float totalWeight = max(weightedProbe + weightedSSR, 1e-4f);

    float3 composite =
        (reflColorProbe * weightedProbe + reflColorSSR * weightedSSR) / totalWeight;

    if (g_params3.x > 0.0001f)
    {
        float3 rtReflection = rtReflectionTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;
        composite = lerp(composite, rtReflection, saturate(g_params3.x));
    }

    float roughAtten = 1.0f - roughness;
    float reflectWeight = saturate(g_params0.w) * saturate(g_params2.w) * fresnel * roughAtten;

    float3 outColor = lerp(scene, composite, reflectWeight);
    return float4(outColor, 1.0f);
}
