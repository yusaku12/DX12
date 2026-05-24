#include "Common.hlsli"

Texture2D particleTex : register(t1);

struct PSIn
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 PS(PSIn input) : SV_Target
{
    float4 tex = particleTex.Sample(samplerStates[LINEAR_CLAMP], input.uv);
    float4 color = tex * input.color;
    if (color.a <= 0.001f) discard;
    return color;
}
