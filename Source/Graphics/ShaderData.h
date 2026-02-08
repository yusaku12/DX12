#pragma once
#pragma once

#include <array>

//! シェーダーID
enum class ShaderID : int
{
    TestPolygonVS,
    TestPolygonPS,
    MAX
};

//! シェーダー記述子
struct ShaderDesc
{
    std::wstring path;
    const char* entry;
    const char* profile;
};

//! シェーダーテーブル
static const std::array<ShaderDesc, static_cast<size_t>(ShaderID::MAX)> shaderTable =
{
    ShaderDesc{ L"Shader/PolygonVS.hlsl",  "VS", "vs_5_0" }, //!< TestPolygonVS
    ShaderDesc{ L"Shader/PolygonPS.hlsl",  "PS", "ps_5_0" }, //!< TestPolygonPS
};
