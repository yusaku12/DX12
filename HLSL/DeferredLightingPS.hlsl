#include "PostEffect.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"

Texture2D normalRoughnessTex : register(t1);
Texture2D worldPosAoTex : register(t2);

cbuffer LightParams : register(b1)
{
    float3 lightDirection;
    float lightIntensity;
    float3 lightColor;
    float padding;
};

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float4 baseColor = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv);
    float4 normalRoughness = normalRoughnessTex.Sample(samplerStates[LINEAR_CLAMP], input.uv);
    float4 worldPosAo = worldPosAoTex.Sample(samplerStates[LINEAR_CLAMP], input.uv);

    float3 albedo = baseColor.rgb;
    float3 normal = normalize(normalRoughness.rgb * 2.0f - 1.0f);
    float ao = worldPosAo.a;

    float3 lightDir = normalize(-lightDirection);
    float nDotL = saturate(dot(normal, lightDir));

    float3 diffuse = albedo * lightColor * (lightIntensity * nDotL);
    float3 ambient = albedo * 0.03f * ao;

    float3 color = pow(ambient + diffuse, 1.0f / 2.2f);
    return float4(color, 1.0f);
}
