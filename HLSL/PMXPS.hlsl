#include "PMX.hlsli"
#include "Common.hlsli"

Texture2D<float4> diffuseTex : register(t0);
//Texture2D<float4> toonTex : register(t1);

float4 PS(VS_OUT input) : SV_TARGET
{
    float3 light = normalize(float3(1, -1, 1));
    float3 normal = normalize(input.normal);

    //float diffuseB = saturate(dot(-light, normal));
    //float3 toonDif = toonTex.Sample(samplerStates[POINT_CLAMP], float2(0, 1.0 - diffuseB)).rgb;

    float4 color = diffuseTex.Sample(samplerStates[POINT_WRAP], input.uv);

    color.rgb = color.rgb /** toonDif.rgb*/;

    return color;
}
