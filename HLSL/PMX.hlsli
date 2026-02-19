struct VS_OUT
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Transform : register(b1)
{
    row_major float4x4 world;
};

cbuffer Material : register(b2)
{
    float4 diffuse;
    float4 specular;
    float3 ambient;
};
