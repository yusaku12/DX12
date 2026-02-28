#include "PMX.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    VS_OUT output;

    input.pos = mul(world, input.pos);
    output.svpos = mul(mul(projection, view), input.pos);
    output.uv = input.uv;
    output.normal = input.normal;
    output.ray = normalize(input.pos.xyz - eye);
    return output;

    return output;
}
