#include "DebugPrimitive.hlsli"

float4 PS(VSOUT input) : SV_Target
{
    return input.color;
}
