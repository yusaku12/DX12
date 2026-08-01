#include "FBX.hlsli"
#include "CommonConstants.hlsli"

struct SKINNED_VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float3 previousPosition : PREVIOUS_POSITION;
};

VS_OUT VS(SKINNED_VS_IN input)
{
    VS_OUT output;
    output.svpos = mul(float4(input.position, 1.0f), viewProjection);
    output.worldPos = input.position;
    output.previousWorldPos = input.previousPosition;
    output.normal = normalize(input.normal);
    output.tangent = normalize(input.tangent);
    output.binormal = normalize(cross(output.normal, output.tangent));
    output.uv = input.uv;
    return output;
}
