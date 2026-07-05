#include "GpuEffect.hlsli"

cbuffer SimParams : register(b0)
{
    float deltaTime;
    float totalTime;
    float emitRate;
    float emitRadius;

    uint maxParticles;
    uint emitterType;
    uint randomSeed;
    uint resetAll;

    float spread;
    float coneAngle;
    float coneHeight;
    float _pad0;

    float3 emitterSize;
    float _pad1;

    float3 emitOrigin;
    float _pad2;

    float minLifetime;
    float maxLifetime;
    float minSpeed;
    float maxSpeed;

    float startSize;
    float endSize;
    float drag;
    float startRotationSpeed;

    float stretchFactor;
    float noiseStrength;
    float noiseFrequency;
    float _pad3;

    float3 gravity;
    float _pad4;

    float4 startColor;
    float4 endColor;
};

StructuredBuffer<Particle> particlesIn : register(t0);
RWStructuredBuffer<Particle> particlesOut : register(u0);
RWByteAddressBuffer drawArgsBuffer : register(u1);

uint pcgHash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random01(inout uint state)
{
    state = pcgHash(state);
    return (float)state / 4294967295.0;
}

float3 randomDirection(inout uint state)
{
    float angle = random01(state) * 6.28318530718;
    float y = random01(state) * 2.0 - 1.0;
    float r = sqrt(saturate(1.0 - y * y));
    return float3(r * cos(angle), y, r * sin(angle));
}

float noise1(float3 p)
{
    return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453);
}

void respawnParticle(inout Particle p, uint index)
{
    uint seed = pcgHash(randomSeed ^ (index * 1664525u + 1013904223u));

    float3 localPos = float3(0.0, 0.0, 0.0);
    float3 initialDir = float3(0.0, 1.0, 0.0);

    if (emitterType == 0) // Sphere
    {
        float3 dir = randomDirection(seed);
        float r = emitRadius * pow(random01(seed), 1.0 / 3.0);
        localPos = dir * r;
        initialDir = normalize(lerp(float3(0.0, 1.0, 0.0), dir, spread));
    }
    else if (emitterType == 1) // Box
    {
        float3 dir = float3(
            random01(seed) * 2.0 - 1.0,
            random01(seed) * 2.0 - 1.0,
            random01(seed) * 2.0 - 1.0);
        localPos = dir * emitterSize;
        initialDir = normalize(lerp(float3(0.0, 1.0, 0.0), dir, spread));
    }
    else if (emitterType == 2) // Cone
    {
        float angle = random01(seed) * 6.28318530718;
        float rRatio = random01(seed);
        float r = emitRadius * rRatio;
        float h = random01(seed) * coneHeight;
        localPos = float3(r * cos(angle), h, r * sin(angle));

        float coneSpread = lerp(0.001, sin(radians(coneAngle)), spread);
        float3 radial = float3(cos(angle), 0.0, sin(angle)) * coneSpread;
        initialDir = normalize(float3(0.0, 1.0, 0.0) + radial);
    }
    else // Ring
    {
        float angle = random01(seed) * 6.28318530718;
        localPos = float3(cos(angle), 0.0, sin(angle)) * emitRadius;
        initialDir = normalize(lerp(float3(0.0, 1.0, 0.0), float3(cos(angle), 0.0, sin(angle)), spread));
    }

    float speed = lerp(minSpeed, maxSpeed, random01(seed));

    p.position = emitOrigin + localPos;
    p.velocity = initialDir * speed;
    p.age = 0.0;
    p.lifetime = max(0.001, lerp(minLifetime, maxLifetime, random01(seed)));
    p.size = startSize;
    p.rotation = random01(seed) * 6.28318530718;
    p.rotationSpeed = (random01(seed) * 2.0 - 1.0) * startRotationSpeed;
    p.stretch = 1.0;
    p.color = startColor;
    p.subUvStartFrame = floor(random01(seed) * 64.0);
    p.generation = 0;
    p.padding0 = 0;
    p.padding1 = 0;
}

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;

    if (index == 0)
    {
        drawArgsBuffer.Store(0, 6u);   // VertexCountPerInstance
        drawArgsBuffer.Store(8, 0u);   // StartVertexLocation
        drawArgsBuffer.Store(12, 0u);  // StartInstanceLocation
    }

    if (index >= maxParticles)
    {
        return;
    }

    Particle p = particlesIn[index];

    bool isAlive = (p.lifetime > 0.0) && (p.age < p.lifetime);

    if (resetAll != 0)
    {
        respawnParticle(p, index);
        particlesOut[index] = p;
        uint oldCount = 0;
        drawArgsBuffer.InterlockedAdd(4, 1, oldCount);
        return;
    }

    if (isAlive)
    {
        p.age += deltaTime;
        if (p.age < p.lifetime)
        {
            p.velocity += gravity * deltaTime;
            p.velocity *= max(0.0, 1.0 - drag * deltaTime);

            if (noiseStrength > 0.0)
            {
                float n = noise1(p.position * noiseFrequency + totalTime.xxx) * 2.0 - 1.0;
                p.velocity += normalize(float3(n, -n, n * 0.5)) * noiseStrength * deltaTime;
            }

            p.position += p.velocity * deltaTime;
            p.rotation += p.rotationSpeed * deltaTime;

            float t = saturate(p.age / max(p.lifetime, 0.001));
            p.size = lerp(startSize, endSize, t);
            p.color = lerp(startColor, endColor, t);
            p.stretch = (stretchFactor > 0.0) ? (1.0 + length(p.velocity) * stretchFactor) : 1.0;

            particlesOut[index] = p;
            uint oldCount = 0;
            drawArgsBuffer.InterlockedAdd(4, 1, oldCount);
            return;
        }
    }

    uint spawnSeed = pcgHash(index ^ randomSeed ^ asuint(totalTime));
    float spawnChance = saturate((emitRate * deltaTime) / max(1.0, (float)maxParticles));
    if (random01(spawnSeed) < spawnChance)
    {
        respawnParticle(p, index);
        uint oldCount = 0;
        drawArgsBuffer.InterlockedAdd(4, 1, oldCount);
    }
    else
    {
        p.age = 1.0;
        p.lifetime = 0.0;
        p.size = 0.0;
        p.color = float4(0.0, 0.0, 0.0, 0.0);
    }

    particlesOut[index] = p;
}
