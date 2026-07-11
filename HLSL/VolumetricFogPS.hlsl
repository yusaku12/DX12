#include "PostEffect.hlsli"
#include "Common.hlsli"

Texture2D depthTexture : register(t1);

cbuffer CBuffer : register(b0)
{
    row_major float4x4 g_projection;
    row_major float4x4 g_invProjection;
    row_major float4x4 g_invView;
    float4 g_cameraPos; //!< xyz=camera
    float4 g_cameraNearFar; //!< x=near y=far
    float4 g_lightDir;  //!< xyz=main light dir
    float4 g_fogColor;  //!< rgb=tint
    float4 g_params0;   //!< x=density y=heightFalloff z=maxDistance w=stepCount
    float4 g_params1;   //!< x=anisotropy y=inscatter z=ambient w=atmoStrength
    float4 g_params2;   //!< x=groundHeight y=horizonBoost z=depthFogBias w=blendWeight
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

    int stepCount = clamp((int)round(g_params0.w), 8, 96);
    float stepLength = sceneDistance / max((float)stepCount, 1.0f);

    float transmittance = 1.0f;
    float3 scattering = 0.0f;

    [loop]
    for (int i = 0; i < 96; ++i)
    {
        if (i >= stepCount)
        {
            break;
        }

        float dist = (i + 0.5f) * stepLength;
        float3 samplePos = g_cameraPos.xyz + rayDir * dist;

        float height = max(samplePos.y - g_params2.x, 0.0f);
        float localDensity = g_params0.x * exp(-height * g_params0.y);

        float cosTheta = dot(rayDir, -lightDir);
        float phase = PhaseHG(cosTheta, g_params1.x);

        float horizon = saturate(1.0f - abs(rayDir.y));
        float atmosphere = 1.0f + g_params2.y * horizon;

        float3 inscatter = g_fogColor.rgb * (g_params1.z + g_params1.y * phase * atmosphere) * localDensity;

        float extinction = exp(-localDensity * stepLength);
        scattering += transmittance * inscatter * stepLength;
        transmittance *= extinction;
    }

    float atmoMask = saturate(1.0f - exp(-sceneDistance * g_params1.w * 0.01f));
    float3 fogged = scene * transmittance + scattering * atmoMask;

    float3 outColor = lerp(scene, fogged, blendWeight);
    return float4(outColor, 1.0f);
}
