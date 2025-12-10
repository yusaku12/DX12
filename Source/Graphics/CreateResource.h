#pragma once

#include <d3d12.h>

//! シェーダーリソースビュー作成
D3D12_SHADER_RESOURCE_VIEW_DESC createSRVDesc(DXGI_FORMAT format, UINT mipLv)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = mipLv;
    return srvDesc;
}

//! 定数バッファビュー作成
//template<typename T>
//D3D12_CONSTANT_BUFFER_VIEW_DESC createCBVDesc(ID3D12Resource* resource)
//{
//    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
//
//    //! CBV は 256 バイトアラインが必須
//    const UINT size = static_cast<UINT>(sizeof(T));
//    const UINT alignedSize = (size + 255) & ~255;
//
//    cbvDesc.BufferLocation = resource->GetGPUVirtualAddress();
//    cbvDesc.SizeInBytes = alignedSize;
//
//    return cbvDesc;
//}