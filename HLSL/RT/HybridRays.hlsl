struct Payload
{
    float3 color;
    float hitT;
};

RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D<float4> g_output : register(u0);

[shader("raygeneration")]
void RayGen()
{
    const uint2 launchIndex = DispatchRaysIndex().xy;
    const uint2 launchDim = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5f) / float2(launchDim);
    float2 ndc = uv * 2.0f - 1.0f;

    float3 rayOrigin = float3(0.0f, 0.5f, 0.0f);
    float3 rayDir = normalize(float3(ndc.x, -ndc.y * 0.6f + 0.15f, 1.0f));

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    Payload payload;
    payload.color = float3(0.53f, 0.67f, 0.86f);
    payload.hitT = -1.0f;

    TraceRay(
        g_scene,
        RAY_FLAG_NONE,
        0xFF,
        0,
        1,
        0,
        ray,
        payload);

    float horizon = saturate(1.0f - abs(ndc.y));
    float3 sky = lerp(float3(0.12f, 0.17f, 0.27f), float3(0.62f, 0.75f, 0.92f), saturate(1.0f - ndc.y));
    sky += horizon * 0.08f;

    float3 color = (payload.hitT > 0.0f) ? payload.color : sky;
    g_output[launchIndex] = float4(color, 1.0f);
}

[shader("miss")]
void Miss(inout Payload payload)
{
    // miss では初期値の空色を維持
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 bary = float3(1.0f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    float3 base = float3(0.95f, 0.72f, 0.32f);
    float3 tint = float3(0.2f, 0.35f, 0.65f) * bary.y + float3(0.65f, 0.2f, 0.3f) * bary.z;

    float hitT = RayTCurrent();
    float atten = 1.0f / (1.0f + hitT * 0.18f);

    payload.color = (base + tint) * atten;
    payload.hitT = hitT;
}
