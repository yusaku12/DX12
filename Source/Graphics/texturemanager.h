#pragma once

#include "loadtexture.h"

//=====================================================
// TextureManager クラス
//=====================================================
class TextureManager
{
public:

    static TextureManager& Instance();

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