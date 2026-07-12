#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    row_major float4x4 g_view;
    float4 g_params0; //!< x=intensity y=maxDistance z=thickness w=stepScale
    float4 g_params1; //!< x=near y=far z=maxSteps w=samples
    float4 g_params2; //!< x=blendWeight y=normalWeight z=saturation w=maxRadiance
    float4 g_params3; //!< x=debugMode y=debugScale
};

float LinearizeDepth(float depth)
{
    const float nearZ = g_params1.x;
    const float farZ = g_params1.y;
    return (nearZ * farZ) / max(farZ - depth * (farZ - nearZ), 1e-6f);
}

float3 DecodeViewNormal(float2 uv)
{
    float3 n = normalTexture.SampleLevel(samplerStates[LINEAR_CLAMP], uv, 0).xyz * 2.0f - 1.0f;
    return normalize(mul(float4(n, 0.0f), g_view).xyz);
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

    float3 normal = DecodeViewNormal(input.uv);
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
    float hitCount = 0.0f;

    [loop]
    for (int s = 0; s < 16; ++s)
    {
        if (s >= sampleCount)
        {
            break;
        }

        float sampleT = (s + 0.5f) / sampleCount;
        float phi = 6.2831853f * frac((s + 0.5f) * 0.61803398875f);
        float cosTheta = sqrt(1.0f - sampleT);
        float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
        float3 rayDir = normalize(
            tangent * (cos(phi) * sinTheta) +
            bitangent * (sin(phi) * sinTheta) +
            normal * cosTheta);

        float3 rayOrigin = viewPos + normal * max(0.03f, thickness * 0.5f);

        [loop]
        for (int i = 1; i <= 48; ++i)
        {
            if (i > maxSteps)
            {
                break;
            }

            float t = (i / (float)maxSteps) * maxDistance * stepScale;
            float3 samplePos = rayOrigin + rayDir * t;

            float sampleViewDepth = samplePos.z;
            if (sampleViewDepth <= 0.0f)
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
            float dz = sampleViewDepth - sampleDepthLin;
            float stepLength = maxDistance * stepScale / max((float)maxSteps, 1.0f);
            float hitThickness = max(thickness, stepLength * 1.25f);
            if (dz < 0.0f || dz > hitThickness)
            {
                continue;
            }

            float3 sampleNormal = DecodeViewNormal(sampleUv);
            float nWeight = lerp(1.0f, saturate(dot(normal, sampleNormal)), saturate(g_params2.y));
            float dWeight = 1.0f - saturate(t / maxDistance);

            float3 bounced = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], sampleUv, 0).rgb;
            float3 radiance = bounced * nWeight * dWeight;
            indirectAccum += radiance;
            weightAccum += nWeight * dWeight;
            hitCount += 1.0f;
            break;
        }
    }

    float3 indirect = (weightAccum > 0.0f) ? (indirectAccum / weightAccum) : 0.0f;
    indirect = ApplySaturation(indirect, g_params2.z);

    // 拡散 GI なので滑らかな面の寄与を抑え、粗い面ほど強くする
    float roughWeight = lerp(0.2f, 1.0f, roughness);
    indirect *= roughWeight;

    indirect = min(indirect * g_params0.x, g_params2.w.xxx);

    int debugMode = (int)round(g_params3.x);
    float debugScale = max(g_params3.y, 0.1f);
    if (debugMode == 1)
    {
        float peak = max(max(indirect.r, indirect.g), max(indirect.b, 1.0e-4f));
        float magnitude = 1.0f - exp2(-dot(indirect, float3(0.2126f, 0.7152f, 0.0722f)) * debugScale);
        return float4((indirect / peak) * magnitude, 1.0f);
    }
    if (debugMode == 2)
    {
        float hitRatio = saturate(hitCount / max((float)sampleCount, 1.0f));
        float heat = saturate(hitRatio * debugScale);
        float3 heatColor = (heat < 0.5f)
            ? lerp(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.35f, 1.0f), heat * 2.0f)
            : lerp(float3(0.0f, 0.35f, 1.0f), float3(1.0f, 0.15f, 0.0f), (heat - 0.5f) * 2.0f);
        return float4(heatColor, 1.0f);
    }

    float3 outColor = scene + indirect * saturate(g_params2.x);
    if (debugMode == 3)
    {
        return float4(outColor, 1.0f);
    }

    return float4(outColor, 1.0f);
}
