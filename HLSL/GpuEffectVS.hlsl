#include "GpuEffect.hlsli"
#include "CommonConstants.hlsli"

cbuffer SimParams : register(b1)
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

    // 追加パラメータ (描画用)
    uint renderMode; // 0: Billboard, 1: Stretched, 2: Horizontal, 3: Vertical
    uint flipbookRows;
    uint flipbookCols;
    float flipbookFps;
};

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

    // レンダーモードに応じたワールド座標の計算
    float3 worldPos = p.position;
    float2 local = kPos[vertexId] * p.size;

    // 回転行列の計算（Billboard、Flatで使用）
    float s = sin(p.rotation);
    float c = cos(p.rotation);
    float2 rotated = float2(local.x * c - local.y * s, local.x * s + local.y * c);

    if (renderMode == 0) // 通常のビルボード
    {
        float3 right = float3(view._11, view._12, view._13);
        float3 up = float3(view._21, view._22, view._23);
        worldPos += right * rotated.x + up * rotated.y;
    }
    else if (renderMode == 1) // Stretched Billboard (速度方向に伸びる)
    {
        // 開発者注: 速度ベクトルベースでビルボードを決定する
        float3 velocity = p.velocity;
        float velLen = length(velocity);
        
        float3 up = float3(0, 1, 0);
        float3 right = float3(1, 0, 0);

        if (velLen > 0.001)
        {
            up = velocity / velLen;
            float3 camPos = float3(viewInverse._41, viewInverse._42, viewInverse._43);
            float3 toCam = normalize(camPos - p.position);
            right = normalize(cross(up, toCam));
        }
        else
        {
            right = float3(view._11, view._12, view._13);
            up = float3(view._21, view._22, view._23); // フォールバック
        }

        // 速度に基づいた引き伸ばしスケール
        float sizeY = p.size * p.stretch;
        float sizeX = p.size;

        float2 scaledPos = kPos[vertexId];
        worldPos += right * (scaledPos.x * sizeX) + up * (scaledPos.y * sizeY);
    }
    else if (renderMode == 2) // Horizontal Flat (床面)
    {
        float3 right = float3(1, 0, 0);
        float3 forward = float3(0, 0, 1);
        worldPos += right * rotated.x + forward * rotated.y;
    }
    else if (renderMode == 3) // Vertical Flat (壁面)
    {
        float3 right = float3(1, 0, 0);
        float3 up = float3(0, 1, 0);
        worldPos += right * rotated.x + up * rotated.y;
    }

    // Flipbook アニメーションのUV計算
    float2 baseUv = kUv[vertexId];
    float2 finalUv = baseUv;

    if (flipbookRows > 1 || flipbookCols > 1)
    {
        float t = saturate(p.age / p.lifetime);
        uint totalFrames = flipbookRows * flipbookCols;
        uint currentFrame = 0;

        if (flipbookFps > 0.0f)
        {
            currentFrame = uint(p.age * flipbookFps) % totalFrames;
        }
        else
        {
            currentFrame = uint(t * float(totalFrames));
            if (currentFrame >= totalFrames) currentFrame = totalFrames - 1;
        }

        float2 frameSize = float2(1.0f / float(flipbookCols), 1.0f / float(flipbookRows));
        uint colIdx = currentFrame % flipbookCols;
        uint rowIdx = currentFrame / flipbookCols;

        finalUv = baseUv * frameSize + float2(float(colIdx) * frameSize.x, float(rowIdx) * frameSize.y);
    }

    VSOut o;
    o.svpos = mul(float4(worldPos, 1.0), viewProjection);
    o.uv = finalUv;
    o.color = p.color;
    return o;
}
