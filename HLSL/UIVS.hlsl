//!< UI 頂点シェーダー
//!< スクリーン空間（OrthoProj）およびワールド空間（MVP）に対応

cbuffer UIConstantsCB : register(b0)
{
    row_major float4x4 g_transform;      //!< OrthoProj（スクリーン空間）or MVP（ワールド空間）
    row_major float4x4 g_localTransform; //!< ローカルアニメーション変換
    float4             g_tintColor;      //!< グローバルティント（頂点カラーに乗算）
    uint               g_textureMode;    //!< 0=カラー / 1=RGBAテクスチャ / 2=フォント alpha-only
    float              g_globalAlpha;    //!< フェードトランジション用グローバルアルファ
    float2             g_pad;
};

struct VSInput
{
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

PSInput VS(VSInput input)
{
    PSInput output;
    float4 pos4     = float4(input.position, 0.0f, 1.0f);
    pos4            = mul(pos4, g_localTransform);
    output.position = mul(pos4, g_transform);
    output.texcoord = input.texcoord;
    output.color    = input.color * g_tintColor;
    output.color.a *= g_globalAlpha;
    return output;
}
