#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);
Texture2DArray shadowMapTexture : register(t2);

#define VOLUME_CASCADE_COUNT 4

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_view;
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    row_major float4x4 g_invView;
    float4 g_cameraPos; //!< xyz=camera
    float4 g_cameraNearFar; //!< x=near y=far
    float4 g_lightDir;  //!< xyz=main light dir
    float4 g_lightColorIntensity; //!< rgb=lightColor a=intensity
    float4 g_fogColor;  //!< rgb=tint
    float4 g_params0;   //!< x=density y=heightFalloff z=maxDistance w=stepCount
    float4 g_params1;   //!< x=anisotropy y=inscatter z=ambient w=atmoStrength
    float4 g_params2;   //!< x=groundHeight y=horizonBoost z=depthFogBias w=blendWeight
    float4 g_shadowParams; //!< x=shadowBias y=shadowStrength z=shadowMapSize w=time
    float4 g_qualityParams;//!< x=shadowSoftness y=shadowDistanceFade z=multiScatter w=noiseAmount
    float4 g_cascadeSplits;
    row_major float4x4 g_shadowLightViewProj[VOLUME_CASCADE_COUNT];
};

float LinearizeDepth(float depth)
{
    const float nearZ = g_cameraNearFar.x;
    const float farZ = g_cameraNearFar.y;
    return (nearZ * farZ) / max(farZ - depth * (farZ - nearZ), 1e-6f);
}

float3 ComputeWorldRay(float2 uv)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, 1.0f, 1.0f);
    float4 view = mul(clip, g_invProjection);
    view /= max(view.w, 1e-6f);

    float3 rayView = normalize(view.xyz);
    float3 rayWorld = normalize(mul(float4(rayView, 0.0f), g_invView).xyz);
    return rayWorld;
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

int SelectCascade(float viewDepth)
{
    int cascade = VOLUME_CASCADE_COUNT - 1;
    [unroll]
    for (int i = 0; i < VOLUME_CASCADE_COUNT; ++i)
    {
        if (viewDepth < g_cascadeSplits[i])
        {
            cascade = i;
            break;
        }
    }
    return cascade;
}

float ComputeShadow(float3 worldPos, float viewDepth)
{
    int cascade = SelectCascade(viewDepth);

    float4 shadowClip = mul(float4(worldPos, 1.0f), g_shadowLightViewProj[cascade]);
    float invW = 1.0f / max(shadowClip.w, 1.0e-6f);
    float3 shadowNdc = shadowClip.xyz * invW;
    float2 uv = shadowNdc.xy * float2(0.5f, -0.5f) + 0.5f;

    if (any(uv <= 0.01f) || any(uv >= 0.99f) || shadowNdc.z <= 0.0f || shadowNdc.z >= 1.0f)
    {
        return 1.0f;
    }

    float texelSize = 1.0f / max(g_shadowParams.z, 1.0f);
    float radius = (1.0f + g_qualityParams.x * (0.6f + 0.35f * cascade)) * texelSize;
    float compareDepth = shadowNdc.z - g_shadowParams.x;

    float lit = 0.0f;
    lit += (compareDepth <= shadowMapTexture.SampleLevel(samplerStates[LINEAR_CLAMP], float3(uv + float2(-radius, -radius), cascade), 0).r) ? 1.0f : 0.0f;
    lit += (compareDepth <= shadowMapTexture.SampleLevel(samplerStates[LINEAR_CLAMP], float3(uv + float2( radius, -radius), cascade), 0).r) ? 1.0f : 0.0f;
    lit += (compareDepth <= shadowMapTexture.SampleLevel(samplerStates[LINEAR_CLAMP], float3(uv + float2(-radius,  radius), cascade), 0).r) ? 1.0f : 0.0f;
    lit += (compareDepth <= shadowMapTexture.SampleLevel(samplerStates[LINEAR_CLAMP], float3(uv + float2( radius,  radius), cascade), 0).r) ? 1.0f : 0.0f;
    lit += (compareDepth <= shadowMapTexture.SampleLevel(samplerStates[LINEAR_CLAMP], float3(uv, cascade), 0).r) ? 1.0f : 0.0f;
    lit *= 0.2f;

    float fade = lerp(1.0f, lit, saturate(g_shadowParams.y));
    // 遠方ほどわずかに緩めてノイズを抑制
    float farFade = saturate(viewDepth / max(g_cameraNearFar.y, 1.0f));
    return lerp(fade, 1.0f, farFade * g_qualityParams.y);
}

float PhaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = pow(abs(1.0f + g2 - 2.0f * g * cosTheta), 1.5f);
    return (1.0f - g2) / max(12.5663706f * denom, 1e-4f);
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 scene = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).rgb;

    float blendWeight = saturate(g_params2.w);
    if (blendWeight <= 0.0f)
    {
        return float4(scene, 1.0f);
    }

    float depth = depthTexture.SampleLevel(samplerStates[LINEAR_CLAMP], input.uv, 0).r;
    float sceneDistance = (depth >= 0.99999f) ? g_params0.z : LinearizeDepth(depth);
    sceneDistance = min(sceneDistance * (1.0f + g_params2.z), g_params0.z);

    float3 rayDir = ComputeWorldRay(input.uv);
    float3 lightDir = normalize(g_lightDir.xyz);
    float3 lightColor = g_lightColorIntensity.rgb * g_lightColorIntensity.a;

    int stepCount = clamp((int)round(g_params0.w), 8, 96);
    float stepLength = sceneDistance / max((float)stepCount, 1.0f);

    float jitter = (Hash12(input.uv * 327.0f + g_shadowParams.w) - 0.5f) * g_qualityParams.w;

    float transmittance = 1.0f;
    float3 scattering = 0.0f;

    [loop]
    for (int i = 0; i < 96; ++i)
    {
        if (i >= stepCount)
        {
            break;
        }

        float dist = (i + 0.5f + jitter) * stepLength;
        float3 samplePos = g_cameraPos.xyz + rayDir * dist;
        float3 viewPos = mul(float4(samplePos, 1.0f), g_view).xyz;
        float sampleViewDepth = max(viewPos.z, 0.0f);

        float height = max(samplePos.y - g_params2.x, 0.0f);
        float localDensity = g_params0.x * exp(-height * g_params0.y);

        float cosTheta = dot(rayDir, -lightDir);
        float phase = PhaseHG(cosTheta, g_params1.x);

        float horizon = saturate(1.0f - abs(rayDir.y));
        float atmosphere = 1.0f + g_params2.y * horizon;
        float shadow = ComputeShadow(samplePos, sampleViewDepth);

        float multiScatter = 1.0f + g_qualityParams.z * (1.0f - shadow) * 0.5f;

        float directTerm = g_params1.y * phase * atmosphere * shadow;
        float ambientTerm = g_params1.z * multiScatter;
        float3 inscatter = g_fogColor.rgb * lightColor * (ambientTerm + directTerm) * localDensity;

        float extinction = exp(-localDensity * stepLength);
        scattering += transmittance * inscatter * stepLength;
        transmittance *= extinction;
    }

    float atmoMask = saturate(1.0f - exp(-sceneDistance * g_params1.w * 0.01f));
    float3 fogged = scene * transmittance + scattering * atmoMask;

    float3 outColor = lerp(scene, fogged, blendWeight);
    return float4(outColor, 1.0f);
}
