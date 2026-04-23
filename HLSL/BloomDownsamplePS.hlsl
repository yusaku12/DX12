#include "PostEffect.hlsli"
#include "Common.hlsli"

// 13-tap ダウンサンプル（Call of Duty / Unity 方式）
// バイリニアサンプリングを活用し 5 回のフェッチで 13 点をカバー

cbuffer CBuffer : register(b0)
{
    float2 g_texelSize; //!< ソース解像度の 1/w, 1/h
    float  g_scatter;
    float  g_padding;
};

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float2 uv = input.uv;
    float2 h = g_texelSize * 0.5;

    //! 5 点のバイリニアフェッチ（各フェッチが 2x2 の平均）
    //! 配置:
    //!   A . B
    //!   . C .
    //!   D . E
    float3 A = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-h.x * 2,  h.y * 2)).rgb;
    float3 B = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(h.x * 2,  h.y * 2)).rgb;
    float3 C = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv).rgb;
    float3 D = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-h.x * 2, -h.y * 2)).rgb;
    float3 E = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(h.x * 2, -h.y * 2)).rgb;

    // 4 つの 2x2 ブロックを追加でサンプル（バイリニアで 4 点平均）
    float3 F = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-h.x,  h.y)).rgb;
    float3 G = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(h.x,  h.y)).rgb;
    float3 H = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(-h.x, -h.y)).rgb;
    float3 I = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], uv + float2(h.x, -h.y)).rgb;

    // 中央ブロック x4 + 角 x1 の加重平均
    float3 result = (F + G + H + I) * 0.5;
    result += (A + B + D + E) * 0.125;
    result += C * 0.125;

    return float4(result, 1.0);
}
