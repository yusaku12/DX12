#include "FBX.hlsli"
#include "Common.hlsli"
#include "CommonConstants.hlsli"
#include "MaterialGraphGenerated.hlsli"

Texture2D<float4> diffuseTex : register(t0);
Texture2D<float4> normalTex : register(t1);

float4 PS(VS_OUT input) : SV_TARGET
{
    MaterialGraphResult graph = EvaluateMaterialGraphById((int)graphId, input.uv, diffuse, pbr, diffuseTex, normalTex, samplerStates[LINEAR_WRAP]);
    return float4(graph.baseColor.rgb, graph.alpha);
}
