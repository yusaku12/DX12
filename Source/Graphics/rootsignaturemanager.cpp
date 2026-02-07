#include "pch.h"

void RootSignatureManager::initialize()
{
    buildStandard();
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(RootSignatureType type) const
{
    return m_rootSignatures[static_cast<size_t>(type)].Get();
}

void RootSignatureManager::buildStandard()
{
    auto device = DX12::Instance().getDevice();

    //! RootSignature生成
    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        0,
        0,
        0,
        0,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &blob,
        &error);

    if (FAILED(hr))
    {
        if (error)
            OutputDebugStringA((char*)error->GetBufferPointer());
        assert(false);
    }

    //! Standardのルートシグネチャを生成
    hr = device->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignatures[static_cast<size_t>(RootSignatureType::Standard)])
    );

    assert(SUCCEEDED(hr));
}