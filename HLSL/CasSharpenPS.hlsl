#include "PostEffect.hlsli"
#include "Common.hlsli"

cbuffer CBuffer : register(b0)
{
    float4 g_params0; //!< x=strength y=clampAmount z=texelX w=texelY
};

float luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float4 PS(PostEffectVSOut input) : SV_Target
{
    const float2 texel = g_params0.zw;

    const float3 c = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv).rgb;
    const float3 n = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv + float2(0.0f, -texel.y)).rgb;
    const float3 s = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv + float2(0.0f, texel.y)).rgb;
    const float3 w = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv + float2(-texel.x, 0.0f)).rgb;
    const float3 e = sceneTexture.Sample(samplerStates[LINEAR_CLAMP], input.uv + float2(texel.x, 0.0f)).rgb;

    const float3 mn = min(c, min(min(n, s), min(w, e)));
    const float3 mx = max(c, max(max(n, s), max(w, e)));

    const float3 blur = (n + s + w + e) * 0.25f;
    float3 sharpened = c + (c - blur) * g_params0.x;

    const float3 clampRange = (mx - mn) * g_params0.y;
    sharpened = clamp(sharpened, c - clampRange, c + clampRange);

    // 低コントラスト部の過剰強調を抑える
    const float contrast = abs(luminance(mx) - luminance(mn));
    const float atten = saturate(contrast * 6.0f);
    const float3 outColor = lerp(c, sharpened, atten);

    return float4(outColor, 1.0f);
}
