#include "FBX.hlsli"
#include "Common.hlsli"

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

    //! テクスチャ
    float4 baseColor = baseColorTex.Sample(samplerStates[LINEAR_WRAP], input.uv) * diffuse;
    float3 normalTS = normalTex.Sample(samplerStates[LINEAR_WRAP], input.uv).xyz * 2.0f - 1.0f;

    //! 法線変換
    float3x3 tbn = float3x3(input.tangent, input.binormal, input.normal);
    float3 normal = normalize(mul(normalTS, tbn));

    //! PBR パラメータ
    float metallic = saturate(pbr.x);
    float roughness = saturate(pbr.y);
    float ao = saturate(pbr.z);

    //! アルベド（線形化）
    float3 albedo = pow(baseColor.rgb, 2.2f);

    output.baseColor = float4(albedo, metallic);
    output.normalRoughness = float4(normal * 0.5f + 0.5f, roughness);
    output.worldPosAo = float4(input.worldPos, ao);

    return output;
}
