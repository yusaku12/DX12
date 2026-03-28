#include "FBX.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"

Texture2D<float4> diffuseTex : register(t0);
Texture2D<float4> normalTex : register(t1);

float4 PS(VS_OUT input) : SV_TARGET
{
    //   float3 lightDir = normalize(float3(0, 0, -1));
    //
    //// TBN構築
    //float3 N = normalize(input.normal);
    //float3 T = normalize(input.tangent.xyz);
    //T = normalize(T - dot(T, N) * N);
    //float3 B = cross(N, T);
    //
    //float3x3 TBN = float3x3(T, B, N);
    //
    //// 法線マップ
    //float3 normalMap = normalTex.Sample(samplerStates[LINEAR_WRAP], input.uv).rgb;
    //normalMap = normalMap * 2.0f - 1.0f;
    //float3 normal = normalize(mul(normalMap, TBN));
    //
    //// ランバート
    //float NdotL = saturate(dot(normal, -lightDir));
    //float3 diffuseColor = diffuse.rgb * NdotL;
    //
    // テクスチャ
    float4 texColor = diffuseTex.Sample(samplerStates[LINEAR_WRAP], input.uv) * diffuse;
//
//// 合成
//float3 finalColor = texColor.rgb * (diffuseColor);
return texColor;
}
