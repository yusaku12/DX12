#include "FBX.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"

Texture2D<float4> baseColorTex : register(t0);
Texture2D<float4> normalTex : register(t1);
TextureCube irradianceTex : register(t2);
TextureCube prefilterTex : register(t3);

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

float3 ToneMapACES(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
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

float4 PS(VS_OUT input) : SV_TARGET
{
    //! テクスチャ
    float4 baseColor = baseColorTex.Sample(samplerStates[LINEAR_WRAP], input.uv) * diffuse;
    float3 normalTS = normalTex.Sample(samplerStates[LINEAR_WRAP], input.uv).xyz * 2.0f - 1.0f;

    //! 法線変換
    float3x3 tbn = float3x3(input.tangent, input.binormal, input.normal);
    float3 normal = normalize(mul(normalTS, tbn));

    //! ビュー / ライト
    float3 viewDir = normalize(eye - input.worldPos);
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float3 halfVec = normalize(viewDir + lightDir);

    //! PBR パラメータ
    float metallic = saturate(pbr.x);
    float roughness = saturate(pbr.y);
    float ao = saturate(pbr.z);

    //! アルベド（線形化）
    float3 albedo = pow(baseColor.rgb, 2.2f);

    //! BRDF
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

    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3 radiance = lightColor;

    float3 direct = (diffuseTerm + specular) * radiance * NdotL;

    //! IBL
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

    float3 color = ambient + direct;

    //! トーンマップ + ガンマ
    color = ToneMapACES(color);
    color = pow(color, 1.0f / 2.2f);

    return float4(color, baseColor.a); }
