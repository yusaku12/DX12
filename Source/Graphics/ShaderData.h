#pragma once

#include <array>

//--------------------------------------------------------------------------------
//! シェーダーID
//--------------------------------------------------------------------------------
enum class ShaderID : int
{
    FBXVS,
    FBXPS,
    GBufferPS,
    DeferredLightingPS,
    DebugPrimitiveVS,
    DebugPrimitivePS,
    PostEffectVS,
    BloomPrefilterPS,
    BloomDownsamplePS,
    BloomUpsamplePS,
    BloomCompositePS,
    DepthOfFieldPS,
    MotionBlurPS,
    ColorGradingPS,
    SkyboxVS,
    SkyboxPS,
    MAX
};

//--------------------------------------------------------------------------------
//! シェーダー記述子
//--------------------------------------------------------------------------------
struct ShaderDesc
{
    std::wstring path = {};
    const char* entry = {};
    const char* profile = {};
};

//--------------------------------------------------------------------------------
//! シェーダーテーブル
//--------------------------------------------------------------------------------
static const std::array<ShaderDesc, static_cast<size_t>(ShaderID::MAX)> shaderTable =
{
    ShaderDesc{ L"Shader/FBXVS.hlsl",              "VS", "vs_5_0" },  //!< FBXVS
    ShaderDesc{ L"Shader/FBXPS.hlsl",              "PS", "ps_5_0" },  //!< FBXPS
    ShaderDesc{ L"Shader/GBufferPS.hlsl",          "PS", "ps_5_0" },  //!< GBufferPS
    ShaderDesc{ L"Shader/DeferredLightingPS.hlsl", "PS", "ps_5_0" },  //!< DeferredLightingPS
    ShaderDesc{ L"Shader/DebugPrimitiveVS.hlsl",   "VS", "vs_5_0" },  //!< DebugVS
    ShaderDesc{ L"Shader/DebugPrimitivePS.hlsl",   "PS", "ps_5_0" },  //!< DebugPS
    ShaderDesc{ L"Shader/PostEffectVS.hlsl",       "VS", "vs_5_0" },  //!< PostEffectVS
    ShaderDesc{ L"Shader/BloomPrefilterPS.hlsl",   "PS", "ps_5_0" },  //!< BloomPrefilterPS
    ShaderDesc{ L"Shader/BloomDownsamplePS.hlsl",  "PS", "ps_5_0" },  //!< BloomDownsamplePS
    ShaderDesc{ L"Shader/BloomUpsamplePS.hlsl",    "PS", "ps_5_0" },  //!< BloomUpsamplePS
    ShaderDesc{ L"Shader/BloomCompositePS.hlsl",   "PS", "ps_5_0" },  //!< BloomCompositePS
    ShaderDesc{ L"Shader/DepthOfFieldPS.hlsl",     "PS", "ps_5_0" },  //!< DepthOfFieldPS
    ShaderDesc{ L"Shader/MotionBlurPS.hlsl",       "PS", "ps_5_0" },  //!< MotionBlurPS
    ShaderDesc{ L"Shader/ColorGradingPS.hlsl",     "PS", "ps_5_0" },  //!< ColorGradingPS
    ShaderDesc{ L"Shader/SkyboxVS.hlsl",           "VS", "vs_5_0" },  //!< SkyboxVS
    ShaderDesc{ L"Shader/SkyboxPS.hlsl",           "PS", "ps_5_0" },  //!< SkyboxPS
};