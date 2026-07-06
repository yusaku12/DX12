#include "Common.hlsli"
#include "MaterialGraphGenerated.hlsli"

Texture2D particleTex : register(t1);

cbuffer RenderParams : register(b1)
{
    uint renderMode;
    uint flipbookRows;
    uint flipbookCols;
    float flipbookFps;
    float graphId;
    float metallic;
    float roughness;
    float ao;
};

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

    float3 pbr = float3(metallic, roughness, ao);
    MaterialGraphResult graph = EvaluateParticleGraphById((int)graphId, input.uv, color, pbr, particleTex, particleTex, samplerStates[LINEAR_CLAMP]);
    return float4(graph.baseColor.rgb, graph.alpha * color.a);
}
