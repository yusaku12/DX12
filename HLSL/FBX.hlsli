struct VS_OUT
{
    float4 svpos : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float3 worldPos : WORLD_POSITION;
};

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

cbuffer Transform : register(b1)
{
    row_major float4x4 world;
};

cbuffer Material : register(b2)
{
    float4 diffuse;
    float3 specular;
    float specularPower;
    float3 ambient;
    float _pad0;
    float3 emissive;
    float _pad1;
};
