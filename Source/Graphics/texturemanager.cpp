#include "pch.h"

TextureManager& TextureManager::Instance()
{
    static TextureManager instance;
    return instance;
}

LoadTexture* TextureManager::load(const std::wstring& filePath)
{
    //! �L���b�V���ɑ��݂���΍ė��p
    auto it = m_textureCache.find(filePath);
    if (it != m_textureCache.end())
    {
        return it->second.get();
    }

    //! ���݂��Ȃ��ꍇ �� �V�K���[�h
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