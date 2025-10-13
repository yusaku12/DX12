#include "polygon.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 PS(VS_OUT input) : SV_TARGET
{
    //return float4(tex.Sample(smp, input.texcoord));
    return float4(1, 0, 1, 1);
}