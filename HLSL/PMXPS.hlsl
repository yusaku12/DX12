#include "PMX.hlsli"
#include "Common.hlsli"

Texture2D<float4> diffuseTex : register(t0);
Texture2D<float4> toonTex : register(t1);

float4 PS(VS_OUT input) : SV_TARGET
{
    float3 lightDir = normalize(float3(1, -1, 1));

    float3 normal = normalize(input.normal);

    //! 拡散
    float diffuseB = saturate(dot(-lightDir, normal));
    float3 toonDif = toonTex.Sample(samplerStates[POINT_CLAMP], float2(0, 1.0 - diffuseB)).rgb;

    //! ベースカラー
    float4 color = diffuseTex.Sample(samplerStates[POINT_WRAP], input.uv);
    color.rgb *= toonDif;

    //! スペキュラ
    float3 viewDir = normalize(-input.ray);
    float3 reflectDir = normalize(reflect(lightDir, normal));

    float specularB = pow(saturate(dot(reflectDir, viewDir)), specular.a);
    float3 specularColor = specular.rgb * specularB;

    //! 最終合成
    color.rgb += specularColor;

    return color;
}
