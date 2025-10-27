#pragma once

#include "loadshader.h"

//=====================================================
// ShaderManager クラス
//=====================================================
class ShaderManager
{
public:

    static ShaderManager& Instance();

    //! シェーダ読み込み
    LoadShader* load(const std::wstring& filePath, ShaderType shaderType, D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline);

    //! 個別アンロード
    void unload(const std::wstring& filePath, ShaderType shaderType);

    //! 全シェーダクリア
    void clear();

private:

    ShaderManager() = default;
    ~ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    //! シェーダキャッシュ
    std::unordered_map<ShaderKey, std::unique_ptr<LoadShader>, ShaderKeyHash> m_shaderCache;
};