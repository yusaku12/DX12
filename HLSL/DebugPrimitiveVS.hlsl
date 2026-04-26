#include "DebugPrimitive.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    VS_OUT o;
    float4 pos = float4(input.position, 1.0f);
    float4 worldPos = mul(world, pos);
    o.position = mul(worldPos,viewProjection);
    o.color = input.color;
    return o;
}
