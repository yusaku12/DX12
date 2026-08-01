#include "pch.h"

void RootSignatureManager::initialize()
{
    buildFBXStandard();
    buildGBuffer();
    buildDebugPrimitive();
    buildPostEffect();
    buildPostEffectDepth();
    buildPostEffectDepthShadow();
    buildPostEffectGBuffer();
    buildPostEffectGBufferIBL();
    buildPostEffectGBufferIBLRT();
    buildPostEffectVelocity();
    buildPostEffectTemporal();
    buildBloomComposite();
    buildSkybox();
    buildDeferredLighting();
    buildGpuEffectRender();
    buildGpuEffectCompute();
    buildHiZPyramidCompute();
    buildSkinningCompute();
    buildShadowDepth();
    buildUI();
}

ID3D12RootSignature* RootSignatureManager::getRootSignature(RootSignatureType type) const
{
    return m_rootSignatures[static_cast<size_t>(type)].Get();
}

void RootSignatureManager::buildFBXStandard()
{
    buildModelMaterialSRV(1, RootSignatureType::FBXStandard);
}

void RootSignatureManager::buildGBuffer()
{
    buildModelMaterialSRV(2, RootSignatureType::GBuffer);
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
    buildPostEffectCommon(false);
}

void RootSignatureManager::buildPostEffectDepth()
{
    buildPostEffectCommon(true);
}

void RootSignatureManager::buildPostEffectDepthShadow()
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ SRV (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 深度テクスチャ SRV (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // シャドウマップ Texture2DArray SRV (t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange2;
    srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[3].InitAsDescriptorTable(1, &srvRange2);

    createRootSignature(params, _countof(params), RootSignatureType::PostEffectDepthShadow);
}

void RootSignatureManager::buildPostEffectGBuffer()
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ SRV (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 深度テクスチャ SRV (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // GBuffer Normal/Roughness SRV (t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange2;
    srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[3].InitAsDescriptorTable(1, &srvRange2);

    createRootSignature(params, _countof(params), RootSignatureType::PostEffectGBuffer);
}

void RootSignatureManager::buildPostEffectGBufferIBL()
{
    CD3DX12_ROOT_PARAMETER params[5] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ SRV (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 深度テクスチャ SRV (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // GBuffer Normal/Roughness SRV (t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange2;
    srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[3].InitAsDescriptorTable(1, &srvRange2);

    // IBL Cubemap SRV (t3-t4)
    CD3DX12_DESCRIPTOR_RANGE srvRange3;
    srvRange3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);
    params[4].InitAsDescriptorTable(1, &srvRange3);

    createRootSignature(params, _countof(params), RootSignatureType::PostEffectGBufferIBL);
}

void RootSignatureManager::buildPostEffectGBufferIBLRT()
{
    CD3DX12_ROOT_PARAMETER params[6] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ SRV (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 深度テクスチャ SRV (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // GBuffer Normal/Roughness SRV (t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange2;
    srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[3].InitAsDescriptorTable(1, &srvRange2);

    // IBL Cubemap SRV (t3-t4)
    CD3DX12_DESCRIPTOR_RANGE srvRange3;
    srvRange3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);
    params[4].InitAsDescriptorTable(1, &srvRange3);

    // RayTracing Result SRV (t5)
    CD3DX12_DESCRIPTOR_RANGE srvRange4;
    srvRange4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    params[5].InitAsDescriptorTable(1, &srvRange4);

    createRootSignature(params, _countof(params), RootSignatureType::PostEffectGBufferIBLRT);
}

void RootSignatureManager::buildPostEffectVelocity()
{
    CD3DX12_ROOT_PARAMETER params[3] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ用 SRV テーブル (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 速度テクスチャ用 SRV テーブル (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    createRootSignature(params, _countof(params), RootSignatureType::PostEffectVelocity);
}

void RootSignatureManager::buildPostEffectTemporal()
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ用 SRV テーブル (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 履歴テクスチャ用 SRV テーブル (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // 速度テクスチャ用 SRV テーブル (t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange2;
    srvRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    params[3].InitAsDescriptorTable(1, &srvRange2);

    createRootSignature(params, _countof(params), RootSignatureType::PostEffectTemporal);
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
    CD3DX12_ROOT_PARAMETER params[6] = {};

    // カメラ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // ライト用 CBV (b1)
    params[1].InitAsConstantBufferView(1);

    // GBuffer SRV テーブル (t0-t2)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
    params[2].InitAsDescriptorTable(1, &srvRange);

    // IBL SRV テーブル (t3-t4)
    CD3DX12_DESCRIPTOR_RANGE range1;
    range1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);
    params[3].InitAsDescriptorTable(1, &range1);

    // シャドウパラメータ CBV (b2)
    params[4].InitAsConstantBufferView(2);

    // シャドウマップ SRV テーブル (t5)
    CD3DX12_DESCRIPTOR_RANGE shadowRange;
    shadowRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    params[5].InitAsDescriptorTable(1, &shadowRange);

    // 比較サンプラー付きで生成
    createRootSignatureWithShadowSampler(params, _countof(params), RootSignatureType::DeferredLighting);
}

void RootSignatureManager::buildShadowDepth()
{
    CD3DX12_ROOT_PARAMETER params[2] = {};

    // 光源 VP 行列 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // ボーントランスフォーム CBV (b1)
    params[1].InitAsConstantBufferView(1);

    createRootSignature(params, _countof(params), RootSignatureType::ShadowDepth);
}

void RootSignatureManager::buildUI()
{
    CD3DX12_ROOT_PARAMETER params[2] = {};

    //! b0: 定数バッファ（変換行列 / テクスチャモード / アルファ）
    params[0].InitAsConstantBufferView(0);

    //! t0: テクスチャ SRV
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange);

    createRootSignature(params, _countof(params), RootSignatureType::UI);
}

void RootSignatureManager::buildGpuEffectRender()
{
    CD3DX12_ROOT_PARAMETER params[3] = {};
    params[0].InitAsConstantBufferView(static_cast<int>(CBVType::Camera));
    params[1].InitAsConstantBufferView(1); // register(b1)

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
    params[2].InitAsDescriptorTable(1, &srvRange);

    createRootSignature(params, _countof(params), RootSignatureType::GpuEffectRender);
}

void RootSignatureManager::buildGpuEffectCompute()
{
    CD3DX12_ROOT_PARAMETER params[3] = {};
    params[0].InitAsConstantBufferView(0);

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange);

    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);
    params[2].InitAsDescriptorTable(1, &uavRange);

    createRootSignature(params, _countof(params), RootSignatureType::GpuEffectCompute);
}

void RootSignatureManager::buildHiZPyramidCompute()
{
    CD3DX12_ROOT_PARAMETER params[3] = {};

    // b0: ダウンサンプルパラメータ
    params[0].InitAsConstantBufferView(0);

    // t0: 入力深度（または前ミップ）
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange);

    // u0: 出力ミップ
    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    params[2].InitAsDescriptorTable(1, &uavRange);

    createRootSignature(params, _countof(params), RootSignatureType::HiZPyramidCompute);
}

void RootSignatureManager::buildSkinningCompute()
{
    CD3DX12_ROOT_PARAMETER params[5] = {};
    params[0].InitAsConstants(3, 0);
    params[1].InitAsShaderResourceView(0);
    params[2].InitAsShaderResourceView(1);
    params[3].InitAsShaderResourceView(2);
    params[4].InitAsUnorderedAccessView(0);

    createRootSignature(params, _countof(params), RootSignatureType::SkinningCompute);
}

void RootSignatureManager::buildModelMaterialSRV(UINT srvCount, RootSignatureType type)
{
    CD3DX12_ROOT_PARAMETER params[4] = {};

    // Camera
    params[0].InitAsConstantBufferView(0);

    // Model
    params[1].InitAsConstantBufferView(1);

    // Material
    params[2].InitAsConstantBufferView(2);

    // Texture (SRV)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srvCount, 0);
    params[3].InitAsDescriptorTable(1, &srvRange);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), type);
}

void RootSignatureManager::buildPostEffectCommon(bool useDepth)
{
    if (!useDepth)
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
        return;
    }

    CD3DX12_ROOT_PARAMETER params[3] = {};

    // エフェクトパラメータ用 CBV (b0)
    params[0].InitAsConstantBufferView(0);

    // シーンテクスチャ用 SRV テーブル (t0)
    CD3DX12_DESCRIPTOR_RANGE srvRange0;
    srvRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange0);

    // 深度テクスチャ用 SRV テーブル (t1)
    CD3DX12_DESCRIPTOR_RANGE srvRange1;
    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    params[2].InitAsDescriptorTable(1, &srvRange1);

    // ルートシグネチャ生成
    createRootSignature(params, _countof(params), RootSignatureType::PostEffectDepth);
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

void RootSignatureManager::createRootSignatureWithShadowSampler(const CD3DX12_ROOT_PARAMETER* params, UINT paramCount, RootSignatureType type)
{
    auto device = DX12::Instance().getDevice();

    // 標準 6 サンプラー + 比較サンプラー 1 = 計 7 サンプラー
    constexpr UINT TotalSamplers = static_cast<UINT>(SamplerState::MAX) + 1;
    D3D12_STATIC_SAMPLER_DESC samplers[TotalSamplers] = {};

    const auto* std = PiplineState::Instance().getSamplerStates();
    for (UINT i = 0; i < static_cast<UINT>(SamplerState::MAX); ++i)
        samplers[i] = std[i];

    samplers[static_cast<UINT>(SamplerState::MAX)] = PiplineState::Instance().getShadowComparisonSampler();

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        paramCount,
        params,
        TotalSamplers,
        samplers,
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

    hr = device->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignatures[static_cast<size_t>(type)])
    );

    assert(SUCCEEDED(hr));
}