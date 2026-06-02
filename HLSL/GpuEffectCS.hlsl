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
    float drag;
    uint emitterType;

    float3 emitOrigin;
    float emitRadius;

    float4 startColor;
    float4 endColor;

    float3 gravity;
    float noiseStrength;

    float3 emitterSize;
    float noiseFrequency;

    float coneAngle;
    float coneHeight;
    float minLifetime;
    float maxLifetime;

    float minSpeed;
    float maxSpeed;
    float startRotationSpeed;
    float stretchFactor;
};

ByteAddressBuffer aliveCountBuffer : register(t0);

ConsumeStructuredBuffer<Particle> particlesIn : register(u0);
AppendStructuredBuffer<Particle> particlesOut : register(u1);

uint pcgHash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float hash1u(uint n)
{
    return float(pcgHash(n)) / 4294967295.0;
}

float hash1(float n)
{
    return frac(sin(n) * 43758.5453);
}

float3 hash3(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p * (p.yzx + p.zxy));
}

float noise3(float3 x)
{
    float3 p = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    return lerp(
        lerp(lerp(hash1(n + 0.0), hash1(n + 1.0), f.x),
             lerp(hash1(n + 57.0), hash1(n + 58.0), f.x), f.y),
        lerp(lerp(hash1(n + 113.0), hash1(n + 114.0), f.x),
             lerp(hash1(n + 170.0), hash1(n + 171.0), f.x), f.y), f.z);
}

float3 curlNoise(float3 p)
{
    const float e = 0.1;
    float3 dx = float3(e, 0.0, 0.0);
    float3 dy = float3(0.0, e, 0.0);
    float3 dz = float3(0.0, 0.0, e);

    float p_x0 = noise3(p - dx);
    float p_x1 = noise3(p + dx);
    float p_y0 = noise3(p - dy);
    float p_y1 = noise3(p + dy);
    float p_z0 = noise3(p - dz);
    float p_z1 = noise3(p + dz);

    float x = p_y1 - p_y0 - p_z1 + p_z0;
    float y = p_z1 - p_z0 - p_x1 + p_x0;
    float z = p_x1 - p_x0 - p_y1 + p_y0;

    return normalize(float3(x, y, z) + 0.0001);
}

float3 randomDirection(uint seed)
{
    float a = hash1u(seed) * 6.2831853;
    float b = hash1u(seed + 1u) * 2.0 - 1.0;
    float r = sqrt(saturate(1.0 - b * b));
    return float3(r * cos(a), b, r * sin(a));
}

float3 generateVelocity(uint seed, float3 dir)
{
    float spd = lerp(minSpeed, maxSpeed, hash1u(seed));
    return dir * spd;
}

[numthreads(256, 1, 1)]
void CS(uint3 id : SV_DispatchThreadID)
{
    uint aliveCount = aliveCountBuffer.Load(0);
    uint index = id.x;

    // スレッド分割による衝突防止：
    // [0, aliveCount - 1] -> 生存パーティクルの更新
    // [aliveCount, aliveCount + maxSpawn - 1] -> 新規エミット
    uint maxSpawn = (maxParticles > aliveCount) ? min(emitCount, maxParticles - aliveCount) : 0;

    if (index < aliveCount)
    {
        Particle p = particlesIn.Consume();
        p.age += deltaTime;

        if (p.age < p.lifetime)
        {
            // 物理シミュレーション (重力 + 空気抵抗)
            p.velocity += gravity * deltaTime;
            p.velocity *= saturate(1.0f - drag * deltaTime);

            // カオスノイズ (Curl Noise) による揺らぎ
            if (noiseStrength > 0.0f)
            {
                float3 noiseForce = curlNoise(p.position * noiseFrequency + totalTime) * noiseStrength;
                p.velocity += noiseForce * deltaTime;
            }

            p.position += p.velocity * deltaTime;

            // 回転アニメーション
            p.rotation += p.rotationSpeed * deltaTime;

            // クイック補間
            float t = saturate(p.age / p.lifetime);
            p.size = lerp(startSize, endSize, t);
            p.color = lerp(startColor, endColor, t);

            // Stretched Billboard 等で使用する引き伸ばしスケール
            if (stretchFactor > 0.0)
            {
                p.stretch = 1.0 + length(p.velocity) * stretchFactor;
            }
            else
            {
                p.stretch = 1.0;
            }

            particlesOut.Append(p);
        }
    }
    
    // エミット処理：aliveCount以降のスレッドが担当し、更新スレッドと重複しない
    uint emitIndex = index - aliveCount;
    if (index >= aliveCount && emitIndex < maxSpawn)
    {
        // フレームごとに異なるベースシードを生成し、粒子ごとにユニークなシードを確保
        uint frameHash = pcgHash(asuint(totalTime) ^ asuint(deltaTime));
        uint seed = pcgHash(frameHash + emitIndex * 7919u);

        float3 localPos = float3(0, 0, 0);
        float3 initialDir = float3(0, 1, 0);

        // 形状に応じたエミッション
        if (emitterType == 0) // Sphere
        {
            float3 dir = randomDirection(seed);
            float r = emitRadius * hash1u(pcgHash(seed + 100u));
            localPos = dir * r;
            initialDir = normalize(lerp(float3(0, 1, 0), dir, spread));
        }
        else if (emitterType == 1) // Box
        {
            float3 dir = float3(
                hash1u(pcgHash(seed + 200u)) * 2.0 - 1.0,
                hash1u(pcgHash(seed + 201u)) * 2.0 - 1.0,
                hash1u(pcgHash(seed + 202u)) * 2.0 - 1.0
            );
            localPos = dir * emitterSize;
            initialDir = normalize(lerp(float3(0, 1, 0), dir, spread));
        }
        else if (emitterType == 2) // Cone
        {
            float angle = hash1u(pcgHash(seed + 300u)) * 6.2831853;
            float rRatio = hash1u(pcgHash(seed + 301u));
            float r = emitRadius * rRatio;
            float h = hash1u(pcgHash(seed + 302u)) * coneHeight;
            localPos = float3(r * cos(angle), h, r * sin(angle));

            float coneSpread = lerp(0.001, sin(coneAngle * 0.01745329), spread);
            float3 radialDir = float3(cos(angle), 0, sin(angle)) * coneSpread;
            initialDir = normalize(float3(0, 1, 0) + radialDir);
        }
        else if (emitterType == 3) // Ring
        {
            float angle = hash1u(pcgHash(seed + 400u)) * 6.2831853;
            localPos = float3(cos(angle), 0.0, sin(angle)) * emitRadius;
            initialDir = normalize(lerp(float3(0, 1, 0), float3(cos(angle), 0.0, sin(angle)), spread));
        }

        Particle p = (Particle)0;
        p.position = emitOrigin + localPos;
        p.velocity = generateVelocity(pcgHash(seed + 500u), initialDir);
        p.age = 0.0;
        p.lifetime = lerp(minLifetime, maxLifetime, hash1u(pcgHash(seed + 600u)));
        p.size = startSize;
        p.rotation = hash1u(pcgHash(seed + 700u)) * 6.2831853;
        p.rotationSpeed = (hash1u(pcgHash(seed + 800u)) * 2.0 - 1.0) * startRotationSpeed;
        p.stretch = 1.0;
        p.color = startColor;

        particlesOut.Append(p);
    }
}
