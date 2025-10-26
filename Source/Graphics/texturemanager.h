#pragma once

#include "loadtexture.h"

//=====================================================
// TextureManager �N���X
//=====================================================
class TextureManager
{
public:

    static TextureManager& Instance();

    //! �e�N�X�`�������[�h�i�L���b�V������j
    LoadTexture* load(const std::wstring& filePath);

    //! �S�e�N�X�`�����
    void clear();

private:

    TextureManager() = default;
    ~TextureManager() = default;

    //! �e�N�X�`���L���b�V��
    std::unordered_map<std::wstring, std::unique_ptr<LoadTexture>> m_textureCache;
};