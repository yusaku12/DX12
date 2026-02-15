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

    //! ディスクリプタレンジ
    CD3DX12_DESCRIPTOR_RANGE range = {};

    //! テクスチャ(ディスクリプタレンジ)
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    //! ルートパラメータ
    CD3DX12_ROOT_PARAMETER params[2] = {};

    //! 定数バッファ(カメラ)
    params[0].InitAsConstantBufferView(static_cast<int>(CBVType::Camera));

    //! テクスチャ(ルートパラメータ)
    params[1].InitAsDescriptorTable(1, &range);

    //! RootSignature生成
    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        _countof(params),
        params,
        static_cast<int>(SamplerState::MAX),
        PiplineState::Instance().getSamplerStates(),
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