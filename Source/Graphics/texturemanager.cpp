#include "pch.h"

LoadTexture* TextureManager::load(const std::wstring& filePath)
{
    //! キャッシュに存在すれば再利用
    auto it = m_textureCache.find(filePath);
    if (it != m_textureCache.end())
    {
        return it->second.get();
    }

    //! 存在しない場合 → 新規ロード
    auto newTex = std::make_unique<LoadTexture>(filePath.c_str());
    LoadTexture* texPtr = newTex.get();

    m_textureCache[filePath] = std::move(newTex);

    std::wstring filename = L"Texture Loaded:" + filePath + L"\n";
    LOG_INFO(wstringToString(filename));
    return texPtr;
}

UINT TextureManager::getWhiteTextureSRVIndex()
{
    //! まだ作られていなければロード
    if (m_whiteTexture == nullptr)
    {
        //! ここはプロジェクト内に必ず存在する白テクスチャ
        m_whiteTexture = load(L"Data/Texture/dummyWhite.jpg");
    }

    return m_whiteTexture->getSRVIndex();
}

void TextureManager::clear()
{
    m_textureCache.clear();
    m_whiteTexture = nullptr;
    LOG_INFO("[TextureManager] Cleared all textures");
}