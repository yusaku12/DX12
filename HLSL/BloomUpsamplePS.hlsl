#include "PostEffect.hlsli"
#include "Common.hlsli"

// 9-tap テントフィルター アップサンプル
// scatter で 現在バッファ（dst）との加算ブレンド比を調整

cbuffer CBuffer : register(b0)
{
    float2 g_texelSize; //!< ソース解像度の 1/w, 1/h
    float  g_scatter;   //!< ブレンド係数（0〜1）
    float  g_padding;
};

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float2 uv = input.uv;
    float2 t = g_texelSize;

    //! 3x3 テントフィルター（bilinear 込みで実質 9-tap）
    //!  重み: 1 2 1
    //!        2 4 2  (÷16)
    //!        1 2 1
    float3 result =
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-t.x,  t.y)).rgb * 1.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(0,    t.y)).rgb * 2.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(t.x,  t.y)).rgb * 1.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-t.x,  0)).rgb * 2.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv).rgb * 4.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(t.x,  0)).rgb * 2.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-t.x, -t.y)).rgb * 1.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(0,   -t.y)).rgb * 2.0 +
        sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(t.x, -t.y)).rgb * 1.0;

    result /= 16.0;

    // scatter: 小さい＝鋭いコア、大きい＝広い光輝
    return float4(result * g_scatter, 1.0);
}
