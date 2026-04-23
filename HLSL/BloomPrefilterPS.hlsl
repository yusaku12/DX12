#include "PostEffect.hlsli"
#include "Common.hlsli"

// 輝度閾値 + ソフトニーによる発光部位抽出

cbuffer CBuffer : register(b0)
{
    float  g_threshold;     //!< 閾値
    float  g_knee;          //!< ソフトニー幅
    float2 g_texelSize;     //!< テクセルサイズ（未使用・将来用）
};

//! 相対輝度（ITU-R BT.709）
float luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

//! ソフトニー曲線（Unity URP と同等）
//!   knee = threshold * kneeRatio
float3 quadraticThreshold(float3 c, float threshold, float knee)
{
    float kneeSize = threshold * knee;
    float halfKnee = kneeSize * 0.5;
    float rcp2Knee = 0.5 / (kneeSize + 1e-5);
    float lum = luminance(c);

    float3 result = c;

    // ソフトニー領域 [threshold - knee, threshold + knee]
    float lo = threshold - kneeSize;
    float hi = threshold + kneeSize;

    if (lum < lo)
    {
        result = 0.0;
    }
    else if (lum < hi)
    {
        // 二次曲線でブレンド
        float t = lum - lo;
        float w = (t * t) * rcp2Knee;
        result *= w / max(lum, 1e-5);
    }
    else
    {
        // 閾値超え: 閾値分を差し引いてそのまま通す
        result -= threshold;
    }

    return max(result, 0.0);
}

float4 PS(PostEffectVSOut input) : SV_TARGET
{
    float3 color = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    float3 bloom = quadraticThreshold(color, g_threshold, g_knee);
    return float4(bloom, 1.0);
}
