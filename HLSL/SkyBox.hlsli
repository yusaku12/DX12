struct VS_IN
{
    float3 pos : POSITION;
};

struct VS_OUT
{
    float4 svpos : SV_POSITION;
    float3 dir : TEXCOORD0;
};
