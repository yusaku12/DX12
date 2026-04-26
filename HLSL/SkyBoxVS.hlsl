#include "Skybox.hlsli"
#include "CommonConstants.hlsli"

VS_OUT VS(VS_IN input)
{
    float4 pos = float4(input.pos, 1.0f);

    float4x4 viewNoTranslation = view;
    viewNoTranslation._41 = 0.0f;
    viewNoTranslation._42 = 0.0f;
    viewNoTranslation._43 = 0.0f;

    float4 viewPos = mul(pos, viewNoTranslation);
    float4 clip = mul(viewPos, projection);

    clip.z = clip.w;

    VS_OUT vout;
    vout.svpos = clip;
    vout.dir = input.pos;
    return vout;
}
