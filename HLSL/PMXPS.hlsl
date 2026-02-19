#include "PMX.hlsli"
#include "Common.hlsli"

Texture2D<float4> tex : register(t0);

float4 PS(VS_OUT input) : SV_TARGET
{
    float4 color = tex.Sample(samplerStates[POINT_WRAP], input.uv);
    return color;
}
