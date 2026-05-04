#include "FBX.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"

Texture2D<float4> diffuseTex : register(t0);

float4 PS(VS_OUT input) : SV_TARGET
{
    // テクスチャ
    float4 texColor = diffuseTex.Sample(samplerStates[LINEAR_WRAP], input.uv) * diffuse;

    return texColor;
}
