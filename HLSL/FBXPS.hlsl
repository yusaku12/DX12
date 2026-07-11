#include "FBX.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"

Texture2D<float4> diffuseTex : register(t0);
Texture2D<float4> normalTex : register(t1);

float4 PS(VS_OUT input) : SV_TARGET
{
    float4 texColor = diffuseTex.Sample(samplerStates[LINEAR_WRAP], input.uv);
    return float4((texColor * diffuse).rgb, diffuse.a * texColor.a);
}
