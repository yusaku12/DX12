#include "FBX.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    VS_OUT output;

    float4 worldPos = mul(world, input.pos);
    output.svpos = mul(mul(projection, view), worldPos);
    output.normal = mul((float3x3) world, input.normal);
    output.tangent = float4(mul((float3x3) world, input.tangent.xyz), input.tangent.w);
    output.uv = input.uv;
    output.worldPos = worldPos.xyz;

    return output;
}
