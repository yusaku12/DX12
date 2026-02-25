#include "DebugPrimitive.hlsli"
#include "CommonConstants.hlsli"

VSOUT VS(VSIN input)
{
    VSOUT o;
    float4 pos = float4(input.position, 1.0f);
    o.position = mul(mul(projection, view), pos);
    o.color = input.color;
    return o;
}
