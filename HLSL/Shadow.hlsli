// Shadow.hlsli — Cascaded Shadow Maps (CSM) サンプリングユーティリティ
// Unity / Unreal 準拠の 4 カスケード方式
// PCSS + Contact Shadow 対応

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
    float              shadowMapSize;                  //!< シャドウマップ解像度
    float              pcssLightRadius;                //!< PCSS ライト半径

    float              pcssMinFilterRadius;            //!< PCSS 最小フィルタ半径（texel）
    float              pcssMaxFilterRadius;            //!< PCSS 最大フィルタ半径（texel）
    float              pcssBlockerSearchRadius;        //!< PCSS ブロッカー探索半径（texel）
    float              pcssCascadeScale;               //!< 遠方カスケード半径スケール

    float              contactShadowLength;            //!< 接触影レイ長
    float              contactShadowStrength;          //!< 接触影強度
    float              contactShadowDepthBias;         //!< 接触影深度バイアス
    float              contactShadowNormalBias;        //!< 接触影法線バイアス

    float              contactShadowStepCount;         //!< 接触影ステップ数
    float3             shadowPadding;
};

Texture2DArray         shadowMaps    : register(t5);   //!< 4 スライスのシャドウマップ配列
SamplerComparisonState shadowSampler : register(s6);   //!< 比較サンプラー（PCF/PCSS 用）

static const int POISSON_COUNT = 12;
static const float2 kPoissonDisk[POISSON_COUNT] =
{
    float2(-0.326f, -0.406f),
    float2(-0.840f, -0.074f),
    float2(-0.696f,  0.457f),
    float2(-0.203f,  0.621f),
    float2( 0.962f, -0.195f),
    float2( 0.473f, -0.480f),
    float2( 0.519f,  0.767f),
    float2( 0.185f, -0.893f),
    float2( 0.507f,  0.064f),
    float2( 0.896f,  0.412f),
    float2(-0.322f, -0.933f),
    float2(-0.792f, -0.598f)
};

bool ProjectToShadow(float3 worldPos, int cascade, out float2 uv, out float receiverDepth)
{
    float4 shadowNDC = mul(float4(worldPos, 1.0f), lightViewProj[cascade]);
    shadowNDC.xyz /= max(shadowNDC.w, 1.0e-6f);

    uv = shadowNDC.xy * float2(0.5f, -0.5f) + 0.5f;
    receiverDepth = shadowNDC.z;

    // カスケード境界付近の誤差を避けるため安全マージンを持たせる
    return all(uv > 0.01f) && all(uv < 0.99f) && receiverDepth >= 0.0f && receiverDepth <= 1.0f;
}

float SampleShadowPCF(float2 uv, float receiverDepth, int cascade, float filterRadiusTexel)
{
    float texelSize = 1.0f / max(shadowMapSize, 1.0f);
    float2 radius = filterRadiusTexel * texelSize;

    float lit = 0.0f;
    [unroll]
    for (int i = 0; i < POISSON_COUNT; ++i)
    {
        float2 suv = uv + kPoissonDisk[i] * radius;
        lit += shadowMaps.SampleCmpLevelZero(
            shadowSampler,
            float3(suv, (float)cascade),
            receiverDepth);
    }

    return lit / POISSON_COUNT;
}

float ComputePCSS(float3 worldPos, int cascade)
{
    float2 uv;
    float receiverDepth;
    if (!ProjectToShadow(worldPos, cascade, uv, receiverDepth))
    {
        return 1.0f;
    }

    float receiver = receiverDepth - shadowBias;
    float texelSize = 1.0f / max(shadowMapSize, 1.0f);

    float cascadeMul = 1.0f + pcssCascadeScale * cascade;
    float searchRadiusTexel = pcssBlockerSearchRadius * cascadeMul;
    float2 searchRadius = searchRadiusTexel * texelSize;

    float blockerDepthSum = 0.0f;
    float blockerCount = 0.0f;

    // 1) ブロッカー探索
    [unroll]
    for (int i = 0; i < POISSON_COUNT; ++i)
    {
        float2 suv = uv + kPoissonDisk[i] * searchRadius;
        float sampleDepth = shadowMaps.SampleLevel(
            samplerStates[LINEAR_CLAMP],
            float3(suv, (float)cascade),
            0).r;

        if (sampleDepth < receiver)
        {
            blockerDepthSum += sampleDepth;
            blockerCount += 1.0f;
        }
    }

    if (blockerCount < 0.5f)
    {
        return 1.0f;
    }

    float avgBlockerDepth = blockerDepthSum / blockerCount;

    // 2) ペナンブラ推定
    float penumbra = ((receiver - avgBlockerDepth) / max(avgBlockerDepth, 1.0e-4f)) * pcssLightRadius;
    float filterRadiusTexel = pcssMinFilterRadius + penumbra * shadowMapSize;
    filterRadiusTexel = clamp(filterRadiusTexel, pcssMinFilterRadius, pcssMaxFilterRadius);

    // 3) 可変半径 PCF
    return SampleShadowPCF(uv, receiver, cascade, filterRadiusTexel);
}

float ComputeContactShadow(float3 worldPos, float3 normal, float3 lightDir, int cascade)
{
    if (contactShadowStrength <= 0.0001f || contactShadowLength <= 0.0001f)
    {
        return 1.0f;
    }

    int steps = clamp((int)round(contactShadowStepCount), 2, 10);
    float invSteps = 1.0f / steps;

    float3 rayOrigin = worldPos + normal * contactShadowNormalBias;

    float occluded = 0.0f;
    [loop]
    for (int i = 1; i <= 10; ++i)
    {
        if (i > steps)
        {
            break;
        }

        float t = i * invSteps;
        float3 samplePos = rayOrigin + lightDir * (contactShadowLength * t);

        float2 uv;
        float receiverDepth;
        if (!ProjectToShadow(samplePos, cascade, uv, receiverDepth))
        {
            break;
        }

        float lit = shadowMaps.SampleCmpLevelZero(
            shadowSampler,
            float3(uv, (float)cascade),
            receiverDepth - contactShadowDepthBias);

        // 最初の遮蔽ほど強く効かせる
        float stepWeight = 1.0f - t;
        occluded += (1.0f - lit) * stepWeight;
    }

    float contactOcc = saturate(occluded);
    return 1.0f - contactOcc * saturate(contactShadowStrength);
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
//! viewDepth: カメラからの正の距離（LH 系 viewPos.z）
float computeShadow(float3 worldPos, float viewDepth, float3 normal, float3 lightDir)
{
    int   cascade = SelectCascadeIndex(viewDepth);
    float lit     = ComputePCSS(worldPos, cascade);

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
            float nextLit = ComputePCSS(worldPos, cascade + 1);
            lit = lerp(lit, nextLit, blendFactor);
        }
    }

    // Contact Shadow は近接の微細遮蔽を追加する補助項として適用
    // 完全影領域を過剰に暗くしないよう、ベースがある程度 lit の場合のみ寄与を与える
    if (lit > 0.05f)
    {
        float contactLit = ComputeContactShadow(worldPos, normal, lightDir, cascade);
        lit *= contactLit;
    }

    return lerp(1.0f, lit, shadowStrength);
}
