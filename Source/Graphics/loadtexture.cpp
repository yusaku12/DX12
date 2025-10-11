#include "pch.h"
#include "loadtexture.h"

void loadTexture()
{
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImg = {};

    HRESULT hr = DirectX::LoadFromWICFile(L"img/textest.png", DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImg);
}