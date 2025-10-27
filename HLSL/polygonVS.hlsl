#include "polygon.hlsli"

VS_OUT VS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
    VS_OUT output;
    output.position = mul(mat, pos);
    output.texcoord = uv;
    return output;
}