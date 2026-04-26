#include "Skybox.hlsli"
#include "Common.hlsli"

TextureCube skyboxTex : register(t0);

cbuffer cbuff1 : register(b1)
{
    float3 skyTint;
    float exposure;
    float rotation;
    float3 padding;
};

float4 PS(VS_OUT input) : SV_TARGET
{
    float3 dir = normalize(input.dir);

    float s, c;
    sincos(rotation, s, c);
    float3 rotatedDir = float3(
        dir.x * c + dir.z * s,
        dir.y,
        -dir.x * s + dir.z * c);

    float3 color = skyboxTex.Sample(samplerStates[ANISOTROPIC_CLAMP], rotatedDir).rgb;
    color *= skyTint * exposure;

    return float4(color, 1.0f);
}
