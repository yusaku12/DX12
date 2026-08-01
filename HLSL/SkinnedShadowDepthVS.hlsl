cbuffer ShadowLightCB : register(b0)
{
    row_major float4x4 lightViewProj;
    float cascadeIndex;
    float3 pad;
};

struct SKINNED_VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float3 previousPosition : PREVIOUS_POSITION;
};

struct VS_OUT_DEPTH
{
    float4 svpos : SV_POSITION;
};

VS_OUT_DEPTH VS(SKINNED_VS_IN input)
{
    VS_OUT_DEPTH output;
    output.svpos = mul(float4(input.position, 1.0f), lightViewProj);
    return output;
}
