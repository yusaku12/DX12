#pragma once

#include <array>

//! シェーダーID
enum class ShaderID : int
{
    TestPolygonVS,
    TestPolygonPS,
    PMXVS,
    PMXPS,
    FBXVS,
    FBXPS,
    DebugPrimitiveVS,
    DebugPrimitivePS,
    MAX
};

//! シェーダー記述子
struct ShaderDesc
{
    std::wstring path = {};
    const char* entry = {};
    const char* profile = {};
};

//! シェーダーテーブル
static const std::array<ShaderDesc, static_cast<size_t>(ShaderID::MAX)> shaderTable =
{
    ShaderDesc{ L"Shader/PolygonVS.hlsl",  "VS", "vs_5_0" },          //!< TestPolygonVS
    ShaderDesc{ L"Shader/PolygonPS.hlsl",  "PS", "ps_5_0" },          //!< TestPolygonPS
    ShaderDesc{ L"Shader/PMXVS.hlsl",  "VS", "vs_5_0" },              //!< PMXVS
    ShaderDesc{ L"Shader/PMXPS.hlsl",  "PS", "ps_5_0" },              //!< PMXPS
    ShaderDesc{ L"Shader/FBXVS.hlsl",  "VS", "vs_5_0" },              //!< FBXVS
    ShaderDesc{ L"Shader/FBXPS.hlsl",  "PS", "ps_5_0" },              //!< FBXPS
    ShaderDesc{ L"Shader/DebugPrimitiveVS.hlsl",  "VS", "vs_5_0" },   //!< DebugVS
    ShaderDesc{ L"Shader/DebugPrimitivePS.hlsl",  "PS", "ps_5_0" },   //!< DebugPS
};