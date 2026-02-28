#pragma once

#include "LoadTexture.h"

//=====================================================
// 読み込んだテクスチャをキャッシュするシングルトン
//=====================================================
class TextureManager
{
public:

    //! シングルトンインスタンス取得
    static TextureManager& Instance()
    {
        static TextureManager instance;
        return instance;
    }

    //! テクスチャをロード（キャッシュあり）
    LoadTexture* load(const std::wstring& filePath);

    //! 全テクスチャ解放
    void clear();

private:

    TextureManager() = default;
    ~TextureManager() = default;

    //! テクスチャキャッシュ
    std::unordered_map<std::wstring, std::unique_ptr<LoadTexture>> m_textureCache;
};