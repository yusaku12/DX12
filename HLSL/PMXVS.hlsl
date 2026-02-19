#include "PMX.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    VS_OUT output;

    input.pos = mul(world, input.pos);
    output.svpos = mul(mul(projection, view), input.pos);
    output.uv = input.uv;
    return output;

    return output;
}
