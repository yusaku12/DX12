#include "pch.h"

void RootSignatureManager::initialize()
{
    buildFBXStandard();
    buildGBuffer();
    buildDebugPrimitive();
    buildPostEffect();
    buildBloomComposite();
    buildSkybox();
    buildDeferredLighting();
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(RootSignatureType type) const
{
    return m_rootSignatures[static_cast<size_t>(type)].Get();
}

void RootSignatureManager::buildFBXStandard()
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // Camera
    params[0].InitAsConstantBufferView(0);

    // Model
    params[1].InitAsConstantBufferView(1);

    // Material
    params[2].InitAsConstantBufferView(2);

    // Texture (SRV)
    CD3DX12_DESCRIPTOR_RANGE range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[3].InitAsDescriptorTable(1, &range);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::FBXStandard);
}

void RootSignatureManager::buildGBuffer()
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // Camera
    params[0].InitAsConstantBufferView(0);

    // Model
    params[1].InitAsConstantBufferView(1);

    // Material
    params[2].InitAsConstantBufferView(2);

    // Texture (SRV)
    CD3DX12_DESCRIPTOR_RANGE range;
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
    params[3].InitAsDescriptorTable(1, &range);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::GBuffer);
}

void RootSignatureManager::buildDebugPrimitive()
{
    // ルートパラメータ
    CD3DX12_ROOT_PARAMETER params[2] = {};

    // 定数バッファ(カメラ)
    params[0].InitAsConstantBufferView(static_cast<int>(CBVType::Camera));

    // 定数バッファ(メッシュ)
    params[1].InitAsConstantBufferView(1); //!< b1

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::DebugPrimitive);
}

void RootSignatureManager::buildPostEffect()
{
    CD3DX12_ROOT_PARAMETER params[2] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ用 SRV テーブル (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::PostEffect);
}

void RootSignatureManager::buildBloomComposite()
{
    // Bloom 合成用: b0(パラメータ) + t0(元シーン) + t1(ブルームテクスチャ)
    CD3DX12_ROOT_PARAMETER params[3] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // 元シーン SRV テーブル (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // ブルームテクスチャ SRV テーブル (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::BloomComposite);
}

void RootSignatureManager::buildSkybox()
{
    CD3DX12_ROOT_PARAMETER params[3] = {};

    // カメラ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // Skybox パラメータ CBV (b1)
    params[1].InitAsConstantBufferView(1);

    // キューブマップ SRV テーブル (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[2].InitAsDescriptorTable(1, &srvRange);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::Skybox);
}

void RootSignatureManager::buildDeferredLighting()
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // カメラ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // ライト用 CBV (b1)
    params[1].InitAsConstantBufferView(1);

    // GBuffer SRV テーブル (t0-t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
    params[2].InitAsDescriptorTable(1, &srvRange);

    // IBL (SRV)
    CD3DX12_DESCRIPTOR_RANGE range1;
    range1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);
    params[3].InitAsDescriptorTable(1, &range1);

    createRootSignature(params, _countof(params), RootSignatureType::DeferredLighting);
}

void RootSignatureManager::createRootSignature(const CD3DX12_ROOT_PARAMETER* params, UINT paramCount, RootSignatureType type)
{
    auto device = DX12::Instance().getDevice();

    // RootSignature生成
    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        paramCount,
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

    // ルートシグネチャを生成
    hr = device->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignatures[static_cast<size_t>(type)])
    );

    assert(SUCCEEDED(hr));
}