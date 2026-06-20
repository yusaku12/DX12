// Shadow.hlsli — Cascaded Shadow Maps (CSM) サンプリングユーティリティ
// Unity / Unreal 準拠の 4 カスケード方式、PCF 3×3 フィルタリング

#define CASCADE_COUNT 4

//=====================================================
//! シャドウパラメータ定数バッファ (b2)
//=====================================================
cbuffer ShadowParams : register(b2)
{
    row_major float4x4 lightViewProj[CASCADE_COUNT];  //!< 各カスケードの光源 VP 行列
    float4             cascadeSplits;                  //!< ビュー空間カスケード分割距離 (正値)
    float              shadowBias;                     //!< 深度バイアス
    float              shadowStrength;                 //!< 影の強さ [0,1]
    float2             shadowPadding;
};

Texture2DArray         shadowMaps    : register(t5);   //!< 4 スライスのシャドウマップ配列
SamplerComparisonState shadowSampler : register(s6);   //!< 比較サンプラー（PCF 用）

// FXC の誤警告回避のため、PCF サンプリングはマクロでインライン展開する。
#define SAMPLE_SHADOW_PCF3X3(_worldPos, _cascade, _result)                                        \
{                                                                                                  \
    float4 _shadowNDC = mul(float4((_worldPos), 1.0f), lightViewProj[(_cascade)]);               \
    _shadowNDC.xyz /= _shadowNDC.w;                                                                \
    float2 _uv = _shadowNDC.xy * float2(0.5f, -0.5f) + 0.5f;                                      \
    if (any(_uv < 0.01f) || any(_uv > 0.99f))                                                      \
    {                                                                                              \
        (_result) = 1.0f;                                                                          \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
        float _depth = _shadowNDC.z - shadowBias;                                                  \
        float _texelSize = 1.0f / 2048.0f;                                                         \
        float _shadow = 0.0f;                                                                      \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2(-1.0f, -1.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2( 0.0f, -1.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2( 1.0f, -1.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2(-1.0f,  0.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2( 0.0f,  0.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2( 1.0f,  0.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2(-1.0f,  1.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2( 0.0f,  1.0f) * _texelSize, (float)(_cascade)), _depth); \
        _shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(_uv + float2( 1.0f,  1.0f) * _texelSize, (float)(_cascade)), _depth); \
        (_result) = _shadow / 9.0f;                                                                \
    }                                                                                              \
}

//! ビュー空間深度からカスケードインデックスを選択
int SelectCascadeIndex(float viewDepth)
{
    int cascade = CASCADE_COUNT - 1;

    [unroll]
    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        if (viewDepth < cascadeSplits[i])
        {
            cascade = i;
            break;
        }
    }

    return cascade;
}

//! 影係数を返す（1.0 = 明るい、0.0 = 完全な影）
//! viewDepth: カメラからの正の距離（右手系 -viewPos.z）
float computeShadow(float3 worldPos, float viewDepth)
{
    int   cascade = SelectCascadeIndex(viewDepth);
    float lit     = 1.0f;
    SAMPLE_SHADOW_PCF3X3(worldPos, cascade, lit);

    // カスケードのつなぎ目を滑らかにするブレンド（Unity/Unreal高品質仕様）
    if (cascade < CASCADE_COUNT - 1)
    {
        float splitDist = cascadeSplits[cascade];
        float prevSplit = (cascade == 0) ? 0.1f : cascadeSplits[cascade - 1]; // ニアプレーン or 前の境界

        // 分割距離の最後の15%の領域で次のカスケードとブレンドする
        float blendRange = max((splitDist - prevSplit) * 0.15f, 1.0e-4f);
        float blendStart = splitDist - blendRange;

        if (viewDepth > blendStart)
        {
            float blendFactor = (viewDepth - blendStart) / blendRange;
            float nextLit = 1.0f;
            SAMPLE_SHADOW_PCF3X3(worldPos, cascade + 1, nextLit);
            lit = lerp(lit, nextLit, blendFactor);
        }
    }

    return lerp(1.0f, lit, shadowStrength);
}

#undef SAMPLE_SHADOW_PCF3X3
