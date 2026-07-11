#include "PostEffect.hlsli"
#include "Common.hlsli"

//!=======================================================
//! 色調補正ピクセルシェーダー
//! URP / Unreal Engine と同等のカラーグレーディング
//!=======================================================

cbuffer CBuffer : register(b0)
{
    // 露出 / ホワイトバランス
    float  g_exposure;              //!< 手動露出 (EV)
    float  g_autoExposureEnabled;   //!< 0:手動 1:自動露出
    float  g_exposureCompensation;  //!< 自動露出補正 (EV)
    float  g_middleGray;            //!< 目標中間輝度

    float  g_minEV;                 //!< 自動露出 下限 EV
    float  g_maxEV;                 //!< 自動露出 上限 EV
    float  g_autoExposureStrength;  //!< 自動露出強度
    float  g_padAuto;

    float  g_temperature;           //!< 色温度シフト  -1..1 (寒→暖)
    float  g_tint;                  //!< ティント      -1..1 (緑→マゼンタ)
    float2 g_pad0;

    // カラー補正
    float  g_contrast;          //!< コントラスト  デフォルト 1.0
    float  g_saturation;        //!< 彩度          デフォルト 1.0
    float  g_hueShift;          //!< 色相シフト  -180..180 deg
    float  g_pad1;

    // シャドウ / ミッドトーン / ハイライト
    float3 g_shadows;           //!< シャドウ色 (RGB リフト)
    float  g_shadowsBalance;    //!< シャドウ影響範囲
    float3 g_midtones;          //!< ミッドトーン色
    float  g_midtonesBalance;
    float3 g_highlights;        //!< ハイライト色
    float  g_highlightsBalance;

};

//-------------------------------------------------------
// ACES Fitted (Krzysztof Narkowicz 近似)
//-------------------------------------------------------
float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

//-------------------------------------------------------
// RGBtoHSV / HSVtoRGB
//-------------------------------------------------------
float3 RGBtoHSV(float3 c)
{
    float4 K = float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float  d = q.x - min(q.w, q.y);
    float  e = 1.0e-10f;
    return float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);
}

float3 HSVtoRGB(float3 c)
{
    float4 K = float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0f - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

//-------------------------------------------------------
// ホワイトバランス（Linear RGB 空間で色温度・ティントを適用）
// D65 → 任意色温度へのシフトを LMS 空間で近似
//-------------------------------------------------------
float3 ApplyWhiteBalance(float3 c, float temperature, float tint)
{
    float t1 = temperature * 0.1f;
    float t2 = tint * 0.1f;

    // D65 基準点
    const float x0 = 0.31271f;
    const float y0 = 0.32902f;

    // シフト後の xy
    float x = x0 - t1 * (t1 < 0.0f ? 0.1f : 0.05f);
    float y = y0 + t2 * 0.05f;

    // balance ゲインを xy から計算（Bradford 近似）
    float3 balance = float3(
        0.949237f * x + 0.354576f,
        -0.023901f * y + 1.000000f,
        (0.831732f - 1.549675f * x) / (1.0f + 10.135853f * x)
    );

    // 中立時（D65）の balance を分母にして差分ゲインのみ適用
    float3 neutral = float3(
        0.949237f * x0 + 0.354576f,
        -0.023901f * y0 + 1.000000f,
        (0.831732f - 1.549675f * x0) / (1.0f + 10.135853f * x0)
    );

    float3 gain = balance / max(neutral, 1e-6f);

    return c * saturate(gain);
}

//-------------------------------------------------------
// Shadows / Midtones / Highlights（Unreal 方式）
//-------------------------------------------------------
float3 ApplyColorWheels(float3 c)
{
    // HDR 過剰値を抑えるため輝度を 0-1 にクランプしてからマスク計算
    float lum = saturate(dot(c, float3(0.2126f, 0.7152f, 0.0722f)));

    // シャドウマスク : 暗部ほど 1
    float shadowMask = saturate(1.0f - lum / max(g_shadowsBalance, 1e-4f));
    // ハイライトマスク : 明部ほど 1
    float highlightMask = saturate((lum - (1.0f - g_highlightsBalance)) / max(g_highlightsBalance, 1e-4f));
    // ミッドトーンマスク : 両端を引いた残り（負にならない）
    float midtoneMask = max(0.0f, 1.0f - shadowMask - highlightMask);

    // 各ゾーンのゲイン : 中立(0,0,0)時は (1,1,1) のまま
    float3 shadows = 1.0f + g_shadows * shadowMask;
    float3 midtones = 1.0f + g_midtones * midtoneMask;
    float3 highlights = 1.0f + g_highlights * highlightMask;

    return max(c * shadows * midtones * highlights, 0.0f);
}

float ComputeApproxSceneLuma(float2 uv)
{
    // 3x3 の広域サンプリングでフレームの代表輝度を近似
    static const float2 kOffsets[9] =
    {
        float2(-0.6f, -0.6f), float2(0.0f, -0.6f), float2(0.6f, -0.6f),
        float2(-0.6f,  0.0f), float2(0.0f,  0.0f), float2(0.6f,  0.0f),
        float2(-0.6f,  0.6f), float2(0.0f,  0.6f), float2(0.6f,  0.6f)
    };

    float lumaSum = 0.0f;
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 suv = saturate(uv + kOffsets[i]);
        float3 s = sceneTexture.SampleLevel(samplerStates[LINEAR_CLAMP], suv, 0).rgb;
        float l = max(dot(s, float3(0.2126f, 0.7152f, 0.0722f)), 1.0e-5f);
        lumaSum += log2(l);
    }

    return exp2(lumaSum / 9.0f);
}

float ComputeExposureEV(float2 uv)
{
    if (g_autoExposureEnabled < 0.5f)
    {
        return g_exposure;
    }

    float avgLuma = ComputeApproxSceneLuma(uv);
    float targetGray = max(g_middleGray, 1.0e-4f);
    float autoEV = log2(targetGray / max(avgLuma, 1.0e-5f));
    autoEV = clamp(autoEV, g_minEV, g_maxEV);

    float blendedEV = lerp(g_exposure, autoEV, saturate(g_autoExposureStrength));
    return blendedEV + g_exposureCompensation;
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    float4 src = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv);
    float3 col = src.rgb;

    // 露出補正（Linear 空間）
    float exposureEV = ComputeExposureEV(input.uv);
    col *= exp2(exposureEV);

    // ホワイトバランス
    col = ApplyWhiteBalance(col, g_temperature, g_tint);

    // コントラスト（簡易 ACEScct 近似）
    {
        const float midGray = 0.18f;
        col = pow(max(col, 1.0e-6f), 1.0f / 2.2f);
        col = (col - midGray) * g_contrast + midGray;
        col = pow(max(col, 1.0e-6f), 2.2f);
    }

    // 彩度
    {
        float lum = dot(col, float3(0.2126f, 0.7152f, 0.0722f));
        col = lerp(float3(lum, lum, lum), col, g_saturation);
    }

    // 色相シフト
    if (abs(g_hueShift) > 0.001f)
    {
        float3 hsv = RGBtoHSV(max(col, 0.0f));
        hsv.x = frac(hsv.x + g_hueShift / 360.0f);
        col = HSVtoRGB(hsv);
    }

    // Shadows / Midtones / Highlights
    col = ApplyColorWheels(col);

    // 統一トーンマップ: ACES
    col = ACESFilm(max(col, 0.0f));

    // 最終出力は sRGB ガンマへ
    col = pow(max(col, 1.0e-6f), 1.0f / 2.2f);

    return float4(col, src.a);
}
