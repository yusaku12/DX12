#include "pch.h"
#include "loadtexture.h"

LoadTexture::LoadTexture(const wchar_t* filename)
{
    loadTexture(filename);
}

void LoadTexture::loadTexture(const wchar_t* filename)
{
    auto device = DX12::Instance().getDevice();

    //! WIC から画像ロード
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImg = {};

    HRESULT hr = DirectX::LoadFromWICFile(
        filename,
        DirectX::WIC_FLAGS_NONE,
        &metadata,
        scratchImg
    );

    if (FAILED(hr))
    {
        Logger::Instance().logCall(LogLevel::ERROR, "Failed to load texture file");
        return;
    }

    //! テクスチャリソース作成
    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Format = metadata.format;
    texDesc.Width = metadata.width;
    texDesc.Height = (UINT)metadata.height;
    texDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
    texDesc.MipLevels = (UINT16)metadata.mipLevels;
    texDesc.SampleDesc.Count = 1;
    texDesc.Dimension = (D3D12_RESOURCE_DIMENSION)metadata.dimension;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(m_texture.GetAddressOf())
    );

    if (FAILED(hr))
    {
        Logger::Instance().logCall(LogLevel::ERROR, "Failed to create texture resource");
        return;
    }

    //! リソースに画像を書き込み
    const DirectX::Image* img = scratchImg.GetImage(0, 0, 0);
    hr = m_texture->WriteToSubresource(
        0,
        nullptr,
        img->pixels,
        img->rowPitch,
        img->slicePitch
    );

    if (FAILED(hr))
    {
        Logger::Instance().logCall(LogLevel::ERROR, "Failed to write texture data");
        return;
    }

    //! SRV 用ディスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap));
    if (FAILED(hr))
    {
        Logger::Instance().logCall(LogLevel::ERROR, "Failed to create SRV heap");
        return;
    }

    //! SRV 作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

    device->CreateShaderResourceView(
        m_texture.Get(),
        &srvDesc,
        m_srvHeap->GetCPUDescriptorHandleForHeapStart()
    );
}