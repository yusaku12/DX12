#include "FBX.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"

Texture2D<float4> diffuseTex : register(t0);
Texture2D<float4> normalTex : register(t1);

float4 PS(VS_OUT input) : SV_TARGET
{
    // テクスチャ
    float4 texColor = diffuseTex.Sample(samplerStates[LINEAR_WRAP], input.uv) * diffuse;
    float3 normal = normalTex.Sample(samplerStates[LINEAR_WRAP], input.uv).xyz * 2.0f - 1.0f;

    // ライティング
     float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
     float3 viewDir = normalize(eye - input.worldPos);
     float3 halfDir = normalize(lightDir + viewDir);
     float NdotL = max(dot(normal, lightDir), 0.0f);
     float NdotH = max(dot(normal, halfDir), 0.0f);
     float3 diffuseComponent = texColor.rgb * NdotL;
     float3 specularComponent = pow(NdotH, 0.5f) * 1.0f;
     return float4(diffuseComponent + specularComponent, texColor.a);
}
