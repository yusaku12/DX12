struct BindPoseVertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
    float4 boneWeights;
    uint4 boneIndices;
};

struct SkinnedVertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
    float3 previousPosition;
};

struct BoneTransform
{
    row_major float4x4 value;
};

cbuffer SkinningParams : register(b0)
{
    uint vertexCount;
    uint hasPreviousOutput;
    uint boneCount;
};

StructuredBuffer<BindPoseVertex> bindPoseVertices : register(t0);
StructuredBuffer<BoneTransform> boneTransforms : register(t1);
StructuredBuffer<SkinnedVertex> previousVertices : register(t2);
RWStructuredBuffer<SkinnedVertex> outputVertices : register(u0);

[numthreads(256, 1, 1)]
void CS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint vertexIndex = dispatchThreadId.x;
    if (vertexIndex >= vertexCount)
    {
        return;
    }

    BindPoseVertex input = bindPoseVertices[vertexIndex];
    const float weightSum = dot(input.boneWeights, 1.0f);
    const float4 normalizedWeights = weightSum > 1.0e-8f
        ? input.boneWeights / weightSum
        : float4(1.0f, 0.0f, 0.0f, 0.0f);
    float3 position = 0.0f;
    float3 normal = 0.0f;
    float3 tangent = 0.0f;

    [unroll]
    for (uint influence = 0; influence < 4; ++influence)
    {
        const float weight = normalizedWeights[influence];
        const uint boneIndex = min(input.boneIndices[influence], boneCount - 1);
        const row_major float4x4 transform = boneTransforms[boneIndex].value;
        position += weight * mul(float4(input.position, 1.0f), transform).xyz;
        normal += weight * mul(float4(input.normal, 0.0f), transform).xyz;
        tangent += weight * mul(float4(input.tangent, 0.0f), transform).xyz;
    }

    SkinnedVertex output;
    output.position = position;
    output.normal = normalize(normal);
    output.tangent = normalize(tangent);
    output.uv = input.uv;
    output.previousPosition = hasPreviousOutput != 0
        ? previousVertices[vertexIndex].position
        : position;
    outputVertices[vertexIndex] = output;
}
