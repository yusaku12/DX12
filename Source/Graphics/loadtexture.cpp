#include "pch.h"
#include "LoadTexture.h"

LoadTexture::LoadTexture(const std::wstring& filePath)
{
    initLoaderTable();
    m_isValid = loadFromFile(filePath);
}

void LoadTexture::initLoaderTable()
{
    m_loaderTable[L"sph"]
        = m_loaderTable[L"spa"]
        = m_loaderTable[L"bmp"]
        = m_loaderTable[L"png"]
        = m_loaderTable[L"jpg"]
        = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
        {
            return LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
        };

    m_loaderTable[L"tga"]
        = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
        {
            return LoadFromTGAFile(path.c_str(), meta, img);
        };

    m_loaderTable[L"dds"]
        = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
        {
            return LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, meta, img);
        };
}

bool LoadTexture::loadFromFile(const std::wstring& filePath)
{
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImg = {};

    //! 拡張子取得
    auto pos = filePath.find_last_of(L'.');
    if (pos == std::wstring::npos)
    {
        LOG_ASSERT_NO_JUDGE("Texture has no extension");
        return false;
    }

    std::wstring ext = filePath.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    auto it = m_loaderTable.find(ext);
    if (it == m_loaderTable.end())
    {
        LOG_ASSERT_NO_JUDGE("Unsupported texture format");
        return false;
    }

    HRESULT hr = it->second(filePath, &metadata, scratchImg);
    if (FAILED(hr))
    {
        //LOG_ASSERT_NO_JUDGE("Texture load failed");
        LOG_ERROR("Texture load failed");
        return false;
    }

    createTextureResource(metadata, scratchImg);
    return true;
}

void LoadTexture::createTextureResource(const DirectX::TexMetadata& meta, const DirectX::ScratchImage& img)
{
    auto device = DX12::Instance().getDevice();

    D3D12_HEAP_PROPERTIES heapProp{};
    heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = (D3D12_RESOURCE_DIMENSION)meta.dimension;
    desc.Format = meta.format;
    desc.Width = meta.width;
    desc.Height = (UINT)meta.height;
    desc.DepthOrArraySize = (UINT16)meta.arraySize;
    desc.MipLevels = (UINT16)meta.mipLevels;
    desc.SampleDesc.Count = 1;

    device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(m_texture.GetAddressOf())
    );

    const DirectX::Image* image = img.GetImage(0, 0, 0);
    m_texture->WriteToSubresource(
        0, nullptr,
        image->pixels,
        (UINT)image->rowPitch,
        (UINT)image->slicePitch
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = meta.format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;

    //! DescriptorHeapManager経由でSRV作成
    m_srvIndex = DescriptorHeapManager::Instance().createSRV(m_texture.Get(), srvDesc);
}