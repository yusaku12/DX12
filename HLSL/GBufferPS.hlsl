#include "FBX.hlsli"
#include "Common.hlsli"
#include "MaterialGraphGenerated.hlsli"

Texture2D<float4> baseColorTex : register(t0);
Texture2D<float4> normalTex : register(t1);

struct GBufferOutput
{
    float4 baseColor : SV_TARGET0;
    float4 normalRoughness : SV_TARGET1;
    float4 worldPosAo : SV_TARGET2;
};

GBufferOutput PS(VS_OUT input)
{
    GBufferOutput output;

    MaterialGraphResult graph = EvaluateMaterialGraphById((int)graphId, input.uv, diffuse, pbr, baseColorTex, normalTex, samplerStates[LINEAR_WRAP]);
    float4 baseColor = graph.baseColor;
    float3 normalTS = normalTex.Sample(samplerStates[LINEAR_WRAP], input.uv).xyz * 2.0f - 1.0f;
    if (graph.hasNormal > 0.5f)
    {
        normalTS = normalize(graph.normalTS);
    }

    //! 法線変換
    float3x3 tbn = float3x3(input.tangent, input.binormal, input.normal);
    float3 normal = normalize(mul(normalTS, tbn));

    //! PBR パラメータ
    float metallic = saturate(graph.metallic);
    float roughness = saturate(graph.roughness);
    float ao = saturate(graph.ao);

    //! アルベド（線形化）
    float3 albedo = pow(baseColor.rgb, 2.2f);

    //! BaseColor の可視化を優先（αは固定）
    output.baseColor = float4(albedo, metallic);
    output.normalRoughness = float4(normal * 0.5f + 0.5f, roughness);
    output.worldPosAo = float4(input.worldPos, ao);

    return output;
}
