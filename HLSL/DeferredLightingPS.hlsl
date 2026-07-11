#include "PostEffect.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"
#include "Shadow.hlsli"

Texture2D normalRoughnessTex : register(t1);
Texture2D worldPosAoTex : register(t2);
TextureCube irradianceTex : register(t3);
TextureCube prefilterTex : register(t4);

cbuffer LightParams : register(b1)
{
    float3 lightDirection;
    float lightIntensity;
    float3 lightColor;
    float padding;
};

static const float PI = 3.14159265f;

float DistributionGGX(float3 normal, float3 halfVec, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(normal, halfVec));
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    return a2 / (PI * denom * denom + 1.0e-5f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 normal, float3 viewDir, float3 lightDir, float roughness)
{
    float NdotV = saturate(dot(normal, viewDir));
    float NdotL = saturate(dot(normal, lightDir));
    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);
    return ggxV * ggxL;
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

float3 EnvBRDFApprox(float3 f0, float roughness, float NdotV)
{
    float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NdotV)) * r.x + r.y;
    float2 ab = float2(-1.04f, 1.04f) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    // テクスチャ
    float3 albedo = sceneTexture.Sample(samplerStates[LINEAR_WRAP], input.uv).rgb;
    float3 normal = normalRoughnessTex.Sample(samplerStates[LINEAR_WRAP], input.uv).xyz * 2.0f - 1.0f;
    normal = normalize(normal);
    float3 worldPos = worldPosAoTex.Sample(samplerStates[LINEAR_WRAP], input.uv).xyz;
    float roughness = normalRoughnessTex.Sample(samplerStates[LINEAR_WRAP], input.uv).w;
    float metallic = sceneTexture.Sample(samplerStates[LINEAR_WRAP], input.uv).w;
    float ao = worldPosAoTex.Sample(samplerStates[LINEAR_WRAP], input.uv).w;

    // ビュー / ライト
    float3 viewDir = normalize(eye - worldPos);
    float3 lightDir = normalize(lightDirection);
    float3 halfVec = normalize(viewDir + lightDir);

    // BRDF
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 F = FresnelSchlick(saturate(dot(halfVec, viewDir)), f0);
    float D = DistributionGGX(normal, halfVec, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);

    float NdotL = saturate(dot(normal, lightDir));
    float NdotV = saturate(dot(normal, viewDir));

    float3 numerator = D * G * F;
    float denom = 4.0f * NdotV * NdotL + 1.0e-5f;
    float3 specular = numerator / denom;

    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);

    float3 diffuseTerm = kD * albedo / PI;

    float3 direct = (diffuseTerm + specular) * lightColor * lightIntensity * NdotL;

    // シャドウ係数（ダイレクトライティングのみに適用、IBL アンビエントは除外）
    float3 viewPos  = mul(float4(worldPos, 1.0f), view).xyz;
    float  viewDepth = viewPos.z;  //!< LH 系: view 空間 Z は正なので反転不要
    float  shadowFactor = computeShadow(worldPos, viewDepth, normal, lightDir);
    direct *= shadowFactor;

    // IBL
    float3 irradiance = irradianceTex.Sample(samplerStates[LINEAR_CLAMP], normal).rgb;

    uint mipLevels = 1;
    uint cubeSize = 1;
    prefilterTex.GetDimensions(0, cubeSize, cubeSize, mipLevels);

    float3 reflectDir = normalize(reflect(-viewDir, normal));
    float lod = roughness * max(1.0f, (float)mipLevels - 1.0f);
    float3 prefiltered = prefilterTex.SampleLevel(samplerStates[LINEAR_CLAMP], reflectDir, lod).rgb;

    float3 specularIBL = prefiltered * EnvBRDFApprox(f0, roughness, NdotV);
    float3 diffuseIBL = irradiance * albedo;

    float3 ambient = (kD * diffuseIBL + specularIBL) * ao;

    // HDR のまま後段の ColorGrading（ACES + Auto Exposure）へ渡す
    float3 color = ambient + direct;
    return float4(max(color, 0.0f), 1.0f);
}
