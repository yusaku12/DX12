#pragma once

#include "LoadShader.h"

//=====================================================
// 読み込んだシェーダをキャッシュするシングルトン
//=====================================================
class ShaderManager
{
public:

    //! シングルトンインスタンス取得
    static ShaderManager& Instance()
    {
        static ShaderManager instance;
        return instance;
    }

    //! シェーダ読み込み
    LoadShader* load(const std::wstring& filePath, ShaderType shaderType);

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