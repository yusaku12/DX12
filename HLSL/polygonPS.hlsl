#include "Polygon.hlsli"
#include "Common.hlsli"

Texture2D<float4> tex : register(t0);

float4 PS(VS_OUT input) : SV_TARGET
{
    return float4(tex.Sample(samplerStates[POINT_WRAP], input.texcoord));
}
