#include "FBX.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    float3 p = 0;
    float3 n = 0;
    float3 t = 0;

    for (int i = 0; i < 4; i++)
    {
        float w = input.boneWeights[i];
        uint idx = input.boneIndices[i];

        float4x4 m = boneTransforms[idx];

        p += w * mul(input.pos, m).xyz;
        n += w * mul(float4(input.normal, 0), m).xyz;
        t += w * mul(float4(input.tangent, 0), m).xyz;
    }

    float4 worldPos = float4(p, 1);

    VS_OUT vout;
    vout.svpos = mul(worldPos, viewProjection);
    vout.worldPos = worldPos.xyz;
    vout.normal = normalize(n);
    vout.tangent = normalize(t);
    vout.binormal = normalize(cross(vout.normal, vout.tangent));
    vout.uv = input.uv;

    return vout;
}
