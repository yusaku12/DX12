struct VS_OUT
{
    float4 svpos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float3 worldPos : WORLD_POSITION;
};

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float4 boneWeights : WEIGHTS;
    uint4 boneIndices : BONES;
};

#define MAX_BONES 256
cbuffer Transform : register(b1)
{
    row_major float4x4 boneTransforms[MAX_BONES];
};

cbuffer Material : register(b2)
{
    float4 diffuse;
    float3 pbr; //!< x: metallic, y: roughness, z: ao
    float graphId;
};
