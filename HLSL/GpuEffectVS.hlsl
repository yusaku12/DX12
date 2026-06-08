#include "GpuEffect.hlsli"
#include "CommonConstants.hlsli"

cbuffer SimParams : register(b1)
{
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

    const float kEps = 0.0001;
    float3 camPos = float3(viewInverse._41, viewInverse._42, viewInverse._43);
    float3 cameraRight = normalize(float3(viewInverse._11, viewInverse._12, viewInverse._13));
    float3 cameraUp = normalize(float3(viewInverse._21, viewInverse._22, viewInverse._23));
    float3 cameraForward = -normalize(float3(viewInverse._31, viewInverse._32, viewInverse._33));

    if (renderMode == 0) // 通常のビルボード
    {
        worldPos += cameraRight * rotated.x + cameraUp * rotated.y;
    }
    else if (renderMode == 1) // Stretched Billboard (速度方向に伸びる)
    {
        // 開発者注: 速度ベクトルベースでビルボードを決定する
        float3 velocity = p.velocity;
        float velLen = length(velocity);
        
        float3 up = float3(0, 1, 0);
        float3 right = cameraRight;

        if (velLen > 0.001)
        {
            up = velocity / velLen;
            float3 toCam = normalize(camPos - p.position);

            float3 rightRaw = cross(up, toCam);
            float rightLen = length(rightRaw);
            if (rightLen > kEps)
            {
                right = rightRaw / rightLen;
            }
            else
            {
                // 速度方向と視線が平行に近い場合のフォールバック
                right = cameraRight;
            }
        }
        else
        {
            right = cameraRight;
            up = cameraUp; // フォールバック
        }

        // 速度に基づいた引き伸ばしスケール
        float sizeY = p.size * p.stretch;
        float sizeX = p.size;

        float2 scaledPos = kPos[vertexId];
        worldPos += right * (scaledPos.x * sizeX) + up * (scaledPos.y * sizeY);
    }
    else if (renderMode == 2) // Horizontal Billboard (床面固定でカメラ方向へ回転)
    {
        float3 worldUp = float3(0, 1, 0);
        float3 toCamFlat = camPos - p.position;
        toCamFlat.y = 0.0;

        float toCamFlatLen = length(toCamFlat);
        float3 forward = (toCamFlatLen > kEps) ? (toCamFlat / toCamFlatLen) : cameraForward;
        forward.y = 0.0;

        float forwardLen = length(forward);
        if (forwardLen > kEps)
        {
            forward /= forwardLen;
        }
        else
        {
            forward = float3(0, 0, 1);
        }

        float3 right = cross(worldUp, forward);
        float rightLen = length(right);
        right = (rightLen > kEps) ? (right / rightLen) : cameraRight;
        right.y = 0.0;
        float rightFlatLen = length(right);
        right = (rightFlatLen > kEps) ? (right / rightFlatLen) : float3(1, 0, 0);

        worldPos += right * rotated.x + forward * rotated.y;
    }
    else if (renderMode == 3) // Vertical Billboard (Y軸固定でカメラ方向へ回転)
    {
        float3 worldUp = float3(0, 1, 0);
        float3 toCam = camPos - p.position;
        toCam.y = 0.0;

        float toCamLen = length(toCam);
        float3 forward = (toCamLen > kEps) ? (toCam / toCamLen) : cameraForward;
        forward.y = 0.0;

        float forwardLen = length(forward);
        if (forwardLen > kEps)
        {
            forward /= forwardLen;
        }
        else
        {
            forward = float3(0, 0, 1);
        }

        float3 right = cross(worldUp, forward);
        float rightLen = length(right);
        right = (rightLen > kEps) ? (right / rightLen) : cameraRight;

        worldPos += right * rotated.x + worldUp * rotated.y;
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
