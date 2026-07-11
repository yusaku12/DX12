cbuffer cbuff0 : register(b0)
{
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 viewProjection;
    row_major float4x4 prevViewProjection;
    row_major float4x4 viewInverse;
    float3 eye;
    float padding0;
};
