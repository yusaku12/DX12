#include "pch.h"
#include "UploadBuffer.h"

UploadBuffer::UploadBuffer(UINT64 size)
{
    create(size);
}

void UploadBuffer::create(UINT64 size)
{
    auto device = DX12::Instance().getDevice();

    CD3DX12_HEAP_PROPERTIES prop(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    HRESULT hr = device->CreateCommittedResource(
        &prop,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_resource.ReleaseAndGetAddressOf())
    );
    LOG_HR(hr, "Failed to create buffer.");
}