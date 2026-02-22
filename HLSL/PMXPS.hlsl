#include "PMX.hlsli"
#include "Common.hlsli"

Texture2D<float4> tex : register(t0);

float4 PS(VS_OUT input) : SV_TARGET
{
    float3 light = normalize(float3(1, -1, 1));
    float3 normal = normalize(input.normal);

    float diffuseB = saturate(dot(-light, normal));
    diffuseB = ceil(diffuseB * 5) / 5.0f;

    float4 color = tex.Sample(samplerStates[POINT_WRAP], input.uv);

    color.rgb = color.rgb /** diffuseB*/;

    return color;
}
