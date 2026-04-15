//! ポストエフェクト共通構造体
struct PostEffectVSOut
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
};

//! シーンテクスチャ (t0)
Texture2D sceneTexture : register(t0);
