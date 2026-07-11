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
    float2 velocity : SV_TARGET3;
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

    // 前フレーム再投影でスクリーン速度を生成（UV差分）。
    float4 currClip = mul(float4(input.worldPos, 1.0f), viewProjection);
    float3 prevWorld = input.worldPos - objectMotion.xyz;
    float4 prevClip = mul(float4(prevWorld, 1.0f), prevViewProjection);

    float2 currNdc = currClip.xy / max(currClip.w, 1.0e-6f);
    float2 prevNdc = prevClip.xy / max(prevClip.w, 1.0e-6f);
    float2 velocityUv = (currNdc - prevNdc) * float2(0.5f, -0.5f);

    // デバッグ表示で可視化しやすいよう 0..1 にエンコードして保持する。
    // 実利用側（TAA/MotionBlur）では同スケールでデコードする。
    const float kVelocityEncodeScale = 8.0f;
    output.velocity = saturate(velocityUv * kVelocityEncodeScale + 0.5f);

    return output;
}
