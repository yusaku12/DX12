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

//! ビュー空間深度からカスケードインデックスを選択
int selectCascade(float viewDepth)
{
    [unroll]
    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        if (viewDepth < cascadeSplits[i])
            return i;
    }
    return CASCADE_COUNT - 1;
}

//! PCF 3×3 シャドウサンプリング
float sampleShadowPCF(float3 worldPos, int cascade)
{
    float4 shadowNDC = mul(float4(worldPos, 1.0f), lightViewProj[cascade]);
    shadowNDC.xyz   /= shadowNDC.w;

    float2 uv = shadowNDC.xy * float2(0.5f, -0.5f) + 0.5f;

    //! カスケード外は完全明るい
    if (any(uv < 0.01f) || any(uv > 0.99f))
        return 1.0f;

    float  depth     = shadowNDC.z - shadowBias;
    float  texelSize = 1.0f / 2048.0f;
    float  shadow    = 0.0f;

    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            shadow += shadowMaps.SampleCmpLevelZero(
                shadowSampler,
                float3(uv + float2(x, y) * texelSize, (float)cascade),
                depth);
        }
    }

    return shadow / 9.0f;
}

//! 影係数を返す（1.0 = 明るい、0.0 = 完全な影）
//! viewDepth: カメラからの正の距離（右手系 -viewPos.z）
float computeShadow(float3 worldPos, float viewDepth)
{
    int   cascade = selectCascade(viewDepth);
    float lit     = sampleShadowPCF(worldPos, cascade);

    // カスケードのつなぎ目を滑らかにするブレンド（Unity/Unreal高品質仕様）
    if (cascade < CASCADE_COUNT - 1)
    {
        float splitDist = cascadeSplits[cascade];
        float prevSplit = (cascade == 0) ? 0.1f : cascadeSplits[cascade - 1]; // ニアプレーン or 前の境界

        // 分割距離の最後の15%の領域で次のカスケードとブレンドする
        float blendRange = (splitDist - prevSplit) * 0.15f; 
        float blendStart = splitDist - blendRange;

        if (viewDepth > blendStart)
        {
            float blendFactor = (viewDepth - blendStart) / blendRange;
            float nextLit     = sampleShadowPCF(worldPos, cascade + 1);
            lit = lerp(lit, nextLit, blendFactor);
        }
    }

    return lerp(1.0f, lit, shadowStrength);
}
