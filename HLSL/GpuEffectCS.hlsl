#include "GpuEffect.hlsli"

cbuffer SimParams : register(b0)
{
    float deltaTime;
    float totalTime;
    float emitRate;
    uint emitCount;

    uint maxParticles;
    float lifetime;
    float speed;
    float spread;

    float startSize;
    float endSize;
    float2 padding0;

    float3 emitOrigin;
    float emitRadius;

    float4 startColor;
    float4 endColor;
};

ByteAddressBuffer aliveCountBuffer : register(t0);

ConsumeStructuredBuffer<Particle> particlesIn : register(u0);
AppendStructuredBuffer<Particle> particlesOut : register(u1);

float hash1(float n)
{
    return frac(sin(n) * 43758.5453);
}

float3 randomDirection(uint seed, float time)
{
    float a = hash1(seed * 12.9898 + time) * 6.2831853;
    float b = hash1(seed * 78.233 + time) * 2.0 - 1.0;
    float r = sqrt(saturate(1.0 - b * b));
    return float3(r * cos(a), b, r * sin(a));
}

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint aliveCount = aliveCountBuffer.Load(0);
    uint index = id.x;

    if (index < aliveCount)
    {
        Particle p = particlesIn.Consume();
        p.age += deltaTime;

        if (p.age < p.lifetime)
        {
            p.position += p.velocity * deltaTime;
            float t = saturate(p.age / p.lifetime);
            p.size = lerp(startSize, endSize, t);
            p.color = lerp(startColor, endColor, t);
            particlesOut.Append(p);
        }
    }

    uint maxSpawn = (maxParticles > aliveCount) ? min(emitCount, maxParticles - aliveCount) : 0;
    if (index < maxSpawn)
    {
        float3 dir = randomDirection(index, totalTime);
        dir = normalize(lerp(float3(0, 1, 0), dir, spread));

        Particle p = (Particle)0;
        p.position = emitOrigin + dir * emitRadius * hash1(index + totalTime);
        p.velocity = dir * speed;
        p.age = 0.0;
        p.lifetime = lifetime;
        p.size = startSize;
        p.rotation = hash1(index * 3.1 + totalTime) * 6.2831853;
        p.color = startColor;

        particlesOut.Append(p);
    }
}
