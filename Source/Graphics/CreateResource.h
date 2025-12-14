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
