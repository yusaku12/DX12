struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer cbuff0 : register(b0)
{
    row_major float4x4 mat;
};