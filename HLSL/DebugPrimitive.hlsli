struct VS_IN
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

//! メッシュ定数バッファ（b1）
cbuffer CbMesh : register(b1)
{
    row_major float4x4 world;
    float4 meshColor;
};
