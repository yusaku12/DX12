#include "pch.h"

TextureManager& TextureManager::Instance()
{
    static TextureManager instance;
    return instance;
}

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

    std::wstring filename = L"[TextureManager] Loaded:" + filePath + L"\n";
    Logger::getInstance().logCall(LogLevel::INFO, wstringToString(filename));
    return texPtr;
}

void TextureManager::clear()
{
    m_textureCache.clear();
    Logger::getInstance().logCall(LogLevel::INFO, "[TextureManager] Cleared all textures");
}