#include "FBX.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    //float4 accumPos = float4(0, 0, 0, 0);
    //float3 accumN = float3(0, 0, 0);
    //float3 accumT = float3(0, 0, 0);

    //// スキニング（最大4ウェイト）
    //for (int i = 0; i < 4; i++)
    //{
    //    float w = input.boneWeights[i];
    //    if (w <= 0.0f) continue;

    //    uint idx = input.boneIndices[i];

    //    float4 tp = mul(float4(input.pos.xyz, 1.0f), boneTransforms[idx]);
    //    accumPos += tp * w;

    //    accumN += mul(float4(input.normal, 0.0f), boneTransforms[idx]).xyz * w;
    //    accumT += mul(float4(input.tangent, 0.0f), boneTransforms[idx]).xyz * w;
    //}

    //VS_OUT vout;

    //float3 p = (accumPos.w != 0.0f) ? accumPos.xyz : input.pos;
    //float3 n = normalize(accumN);
    //float3 t = accumT;
    //if (length(t) > 1e-6f && length(n) > 1e-6f)
    //{
    //    t = t - n * dot(n, t);
    //    t = normalize(t);
    //}
    //else
    //{
    //    t = normalize(input.tangent);
    //}

    //float3 b = normalize(cross(n, t));

    //vout.worldPos = p;
    //vout.normal = n;
    //vout.tangent = t;
    //vout.binormal = b;
    //vout.uv = input.uv;
    //vout.svpos = mul(mul(projection, view), float4(p, 1.0f));

    //return vout;

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
