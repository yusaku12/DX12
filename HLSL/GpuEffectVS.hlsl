#include "GpuEffect.hlsli"
#include "CommonConstants.hlsli"

StructuredBuffer<Particle> particles : register(t0);

struct VSOut
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut VS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    static const float2 kPos[6] =
    {
        float2(-0.5, -0.5),
        float2(0.5, -0.5),
        float2(0.5,  0.5),
        float2(-0.5, -0.5),
        float2(0.5,  0.5),
        float2(-0.5,  0.5),
    };

    static const float2 kUv[6] =
    {
        float2(0, 1),
        float2(1, 1),
        float2(1, 0),
        float2(0, 1),
        float2(1, 0),
        float2(0, 0),
    };

    Particle p = particles[instanceId];

    float3 right = float3(view._11, view._12, view._13);
    float3 up = float3(view._21, view._22, view._23);

    float2 local = kPos[vertexId] * p.size;
    float s = sin(p.rotation);
    float c = cos(p.rotation);
    float2 rotated = float2(local.x * c - local.y * s, local.x * s + local.y * c);

    float3 worldPos = p.position + right * rotated.x + up * rotated.y;

    VSOut o;
    o.svpos = mul(float4(worldPos, 1.0), viewProjection);
    o.uv = kUv[vertexId];
    o.color = p.color;
    return o;
}
