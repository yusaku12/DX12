#include "PostEffect.hlsli"
#include "Common.hlsli"

//!=======================================================
//! 色調補正ピクセルシェーダー
//! URP / Unreal Engine と同等のカラーグレーディング
//!=======================================================

cbuffer CBuffer : register(b0)
{
    // 露出 / ホワイトバランス
    float  g_exposure;          //!< 露出補正 (EV)  デフォルト 0.0
    float  g_temperature;       //!< 色温度シフト  -1..1 (寒→暖)
    float  g_tint;              //!< ティント      -1..1 (緑→マゼンタ)
    float  g_pad0;

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

    // トーンマッピング
    int    g_tonemapMode;       //!< 0=Linear 1=ACES 2=Filmic
    float3 g_pad2;
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
// Hable Filmic（Uncharted 2 方式）
//-------------------------------------------------------
float3 Uncharted2Tonemap(float3 x)
{
    const float A = 0.15f, B = 0.50f, C = 0.10f;
    const float D = 0.20f, E = 0.02f, F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 FilmicTonemap(float3 col)
{
    const float W = 11.2f;
    float3 curr = Uncharted2Tonemap(col * 2.0f);
    float3 whiteScale = 1.0f / Uncharted2Tonemap(float3(W, W, W));
    return curr * whiteScale;
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

float4 PS(PostEffectVSOut input) : SV_Target
{
    float3 col = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;

// 露出補正（Linear 空間）
col *= pow(2.0f, g_exposure);

// ホワイトバランス
col = ApplyWhiteBalance(col, g_temperature, g_tint);

// コントラスト（Log 空間で適用 ≒ ACEScct）
{
    const float midGray = 0.18f;
    col = pow(max(col, 1e-6f), 1.0f / 2.2f);   // Linear → Gamma
    col = (col - midGray) * g_contrast + midGray;
    col = pow(max(col, 1e-6f), 2.2f);           // Gamma → Linear
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

// トーンマッピング
if (g_tonemapMode == 1)
    col = ACESFilm(col);
else if (g_tonemapMode == 2)
    col = FilmicTonemap(col);
else
    col = saturate(col); // Linear クランプ

return float4(col, 1.0f);
}
