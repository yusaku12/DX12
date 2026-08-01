#include "FBX.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    float3 p = 0;
    float3 n = 0;
    float3 t = 0;
    const float weightSum = dot(input.boneWeights, 1.0f);
    const float4 normalizedWeights = weightSum > 1.0e-8f
        ? input.boneWeights / weightSum
        : float4(1.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < 4; i++)
    {
        float w = normalizedWeights[i];
        uint idx = min(input.boneIndices[i], MAX_BONES - 1);

        float4x4 m = boneTransforms[idx];

        p += w * mul(input.pos, m).xyz;
        n += w * mul(float4(input.normal, 0), m).xyz;
        t += w * mul(float4(input.tangent, 0), m).xyz;
    }

    float4 worldPos = float4(p, 1);

    VS_OUT vout;
    vout.svpos = mul(worldPos, viewProjection);
    vout.worldPos = worldPos.xyz;
    vout.previousWorldPos = worldPos.xyz - objectMotion.xyz;
    vout.normal = normalize(n);
    vout.tangent = normalize(t);
    vout.binormal = normalize(cross(vout.normal, vout.tangent));
    vout.uv = input.uv;

    return vout;
}
