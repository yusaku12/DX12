#include "FBX.hlsli"

//=====================================================
//! シャドウ深度パス用頂点シェーダー
//! ボーンスキニングを行い、光源空間へ変換する
//=====================================================

cbuffer ShadowLightCB : register(b0)
{
    row_major float4x4 lightViewProj;  //!< このカスケードの光源 VP 行列
    float              cascadeIndex;
    float3             pad;
};

struct VS_OUT_DEPTH
{
    float4 svpos : SV_POSITION;
};

VS_OUT_DEPTH VS(VS_IN input)
{
    float3 p = 0;
    const float weightSum = dot(input.boneWeights, 1.0f);
    const float4 normalizedWeights = weightSum > 1.0e-8f
        ? input.boneWeights / weightSum
        : float4(1.0f, 0.0f, 0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        const uint boneIndex = min(input.boneIndices[i], MAX_BONES - 1);
        p += normalizedWeights[i] * mul(input.pos, boneTransforms[boneIndex]).xyz;
    }

    VS_OUT_DEPTH vout;
    vout.svpos = mul(float4(p, 1.0f), lightViewProj);
    return vout;
}
