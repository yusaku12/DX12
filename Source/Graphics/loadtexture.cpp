#include "pch.h"
#include "LoadTexture.h"

namespace
{
    std::wstring toLowerExt(const std::wstring& filePath)
    {
        const size_t pos = filePath.find_last_of(L'.');
        if (pos == std::wstring::npos)
        {
            return L"";
        }

        std::wstring ext = filePath.substr(pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        return ext;
    }

    bool decodeByExtension(const std::wstring& filePath,
        DirectX::TexMetadata& metadata,
        DirectX::ScratchImage& scratch,
        std::string* outErrorMessage = nullptr)
    {
        const std::wstring ext = toLowerExt(filePath);
        if (ext.empty())
        {
            if (outErrorMessage)
            {
                *outErrorMessage = "Texture has no extension";
            }
            return false;
        }

        HRESULT hr = E_FAIL;
        if (ext == L"sph" || ext == L"spa" || ext == L"bmp" || ext == L"png" || ext == L"jpg")
        {
            hr = LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratch);
        }
        else if (ext == L"tga")
        {
            hr = LoadFromTGAFile(filePath.c_str(), &metadata, scratch);
        }
        else if (ext == L"dds")
        {
            hr = LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratch);
        }
        else
        {
            if (outErrorMessage)
            {
                *outErrorMessage = "Unsupported texture format";
            }
            return false;
        }

        if (FAILED(hr))
        {
            if (outErrorMessage)
            {
                *outErrorMessage = "Texture decode failed";
            }
            return false;
        }

        return true;
    }
}

LoadTexture::LoadTexture(const std::wstring& filePath)
{
    initLoaderTable();
    m_isValid = loadFromFile(toRelativeWPath(filePath));
}

LoadTexture::~LoadTexture()
{
    releaseGpuResources();
}

LoadTexture::LoadTexture(UINT width, UINT height, DXGI_FORMAT format, const void* pixelData, size_t pixelSize)
{
    auto device = DX12::Instance().getDevice();
    auto cmd = DX12::Instance().getGraphicsCommandList();

    // テクスチャ記述（1配列、1Mip）
    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        format,
        width,
        height,
        1, // arraySize
        1  // mipLevels
    );

    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(m_texture.GetAddressOf())
    );
    LOG_HR(hr, "Failed to create white texture resource");

    // サブリソースデータ準備（1サブリソース）
    D3D12_SUBRESOURCE_DATA subresource{};
    subresource.pData = pixelData;
    // RowPitch はフォーマットに応じて計算（R8G8B8A8_UNORM を想定している場合は width * 4）
    subresource.RowPitch = static_cast<LONG_PTR>(width * (pixelSize / (width * height))); // 保守的にセット
    // 上が計算的に不明瞭なら以下のように幅*bytesPerPixel を直接設定しても良い
    subresource.SlicePitch = subresource.RowPitch * height;

    // UploadBuffer 作成
    UINT64 uploadSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    m_upload = std::make_unique<UploadBuffer>(uploadSize);

    // GPUへコピー
    UpdateSubresources(
        cmd,
        m_texture.Get(),
        m_upload->getResource(),
        0, 0,
        1,
        &subresource
    );

    // SRV用に状態遷移
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmd->ResourceBarrier(1, &barrier);

    // SRV作成（DescriptorHeapManager）
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    m_srvIndex = DescriptorHeapManager::Instance().createSRV(m_texture.Get(), srvDesc);

    m_isValid = true;
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
    DecodedData decoded;
    std::string errorMessage;
    if (!decodeFromFile(filePath, decoded, &errorMessage))
    {
        LOG_ERROR("%s", errorMessage.c_str());
        return false;
    }

    return replaceFromDecoded(std::move(decoded));
}

void LoadTexture::createTextureResource(const DirectX::TexMetadata& meta, const DirectX::ScratchImage& img)
{
    auto device = DX12::Instance().getDevice();
    auto cmd = DX12::Instance().getGraphicsCommandList();

    // GPU側テクスチャ作成（DEFAULT HEAP）
    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        meta.format,
        meta.width,
        (UINT)meta.height,
        (UINT16)meta.arraySize,
        (UINT16)meta.mipLevels
    );

    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);

    device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(m_texture.GetAddressOf())
    );

    // サブリソース準備（全Mip）
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    const DirectX::Image* images = img.GetImages();
    size_t imageCount = img.GetImageCount();

    subresources.resize(imageCount);

    for (size_t i = 0; i < imageCount; ++i)
    {
        subresources[i].pData = images[i].pixels;
        subresources[i].RowPitch = images[i].rowPitch;
        subresources[i].SlicePitch = images[i].slicePitch;
    }

    // UploadBuffer作成
    UINT64 uploadSize = GetRequiredIntermediateSize(m_texture.Get(), 0, (UINT)subresources.size());

    m_upload = std::make_unique<UploadBuffer>(uploadSize);

    // GPUへコピー
    UpdateSubresources(
        cmd,
        m_texture.Get(),
        m_upload->getResource(),
        0, 0,
        (UINT)subresources.size(),
        subresources.data()
    );

    // SRV用に状態遷移
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_texture.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmd->ResourceBarrier(1, &barrier);

    // SRV作成（DescriptorHeapManager）
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = meta.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (meta.IsCubemap())
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = (UINT)meta.mipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    else
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = (UINT)meta.mipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }

    m_srvIndex = DescriptorHeapManager::Instance().createSRV(m_texture.Get(), srvDesc);
}

bool LoadTexture::decodeFromFile(const std::wstring& filePath,
    DecodedData& outDecoded,
    std::string* outErrorMessage)
{
    outDecoded = {};
    const std::wstring resolvedPath = toRelativeWPath(filePath);

    if (!decodeByExtension(resolvedPath, outDecoded.metadata, outDecoded.image, outErrorMessage))
    {
        return false;
    }

    return true;
}

bool LoadTexture::replaceFromDecoded(DecodedData&& decoded)
{
    releaseGpuResources();
    createTextureResource(decoded.metadata, decoded.image);
    m_isValid = (m_texture != nullptr && m_srvIndex != UINT_MAX);
    return m_isValid;
}

void LoadTexture::releaseGpuResources()
{
    if (m_srvIndex != UINT_MAX)
    {
        DescriptorHeapManager::Instance().free(m_srvIndex, 1);
        m_srvIndex = UINT_MAX;
    }

    m_texture.Reset();
    m_upload.reset();
    m_isValid = false;
}