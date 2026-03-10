#include "pch.h"

LoadTexture* TextureManager::load(const std::wstring& filePath)
{
    // キャッシュに存在すれば再利用
    auto it = m_textureCache.find(filePath);
    if (it != m_textureCache.end())
    {
        return it->second.get();
    }

    // 存在しない場合 → 新規ロード
    auto newTex = std::make_unique<LoadTexture>(filePath.c_str());

    // 失敗した場合は白色テクスチャを返す
    if (!newTex->isValid())
    {
        uint8_t whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        newTex = std::make_unique<LoadTexture>(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, whitePixel, sizeof(whitePixel));
    }

    LoadTexture* texPtr = newTex.get();
    m_textureCache[filePath] = std::move(newTex);

    std::wstring filename = L"Texture Loaded:" + filePath + L"\n";
    LOG_INFO(wstringToString(filename));
    return texPtr;
}

void TextureManager::clear()
{
    m_textureCache.clear();
    LOG_INFO("[TextureManager] Cleared all textures");
}