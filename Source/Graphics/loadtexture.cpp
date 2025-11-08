#include "pch.h"
#include "loadtexture.h"
#include "camera.h"

LoadTexture::LoadTexture(const wchar_t* textureName)
{
    loadTexture(textureName);
}

void LoadTexture::applyTexture()
{
    //!@todo 以後修正案件
    //! テクスチャのリソース読み込み
    auto device = DX12::getInstance().getDevice();
    auto commandList = DX12::getInstance().getGraphicsCommandList();
    commandList->SetDescriptorHeaps(1, &m_basicDescHeap);
    commandList->SetGraphicsRootDescriptorTable(0, m_basicDescHeap->GetGPUDescriptorHandleForHeapStart());
    auto heapHandle = m_basicDescHeap->GetGPUDescriptorHandleForHeapStart();
    heapHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    commandList->SetGraphicsRootDescriptorTable(1, heapHandle);
}

void LoadTexture::setRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature)
{
    //!@todo 今後修正案件

    //! テクスチャのルートパラメータ
    D3D12_DESCRIPTOR_RANGE textureDescriptorRange = {};
    textureDescriptorRange.NumDescriptors = 1;
    textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;     //!< SRVなのでこれ
    textureDescriptorRange.BaseShaderRegister = 0;
    textureDescriptorRange.RegisterSpace = 0;
    textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //! 定数バッファのルートパラメータ
    D3D12_DESCRIPTOR_RANGE constantBufferDescriptorRange = {};
    constantBufferDescriptorRange.NumDescriptors = 1;
    constantBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    constantBufferDescriptorRange.BaseShaderRegister = 0;
    constantBufferDescriptorRange.RegisterSpace = 0;
    constantBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //! ルートパラメータ作成
    D3D12_ROOT_PARAMETER rootparam[2] = {};
    rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;           //!< ピクセルシェーダで使用するので
    rootparam[0].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
    rootparam[0].DescriptorTable.NumDescriptorRanges = 1;

    //! 定数バッファ
    rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootparam[1].DescriptorTable.pDescriptorRanges = &constantBufferDescriptorRange;
    rootparam[1].DescriptorTable.NumDescriptorRanges = 1;

    //! ルートシグネチャ(どのシェーダリソースを使用するのか、どのサンプラーを使用するのかを設定するもの)
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;  //!< 頂点情報が存在する
    rootSignatureDesc.pParameters = rootparam;
    rootSignatureDesc.NumParameters = 2;
    D3D12_STATIC_SAMPLER_DESC samplerDesc = setSamplerState(SamplerState::LINEAR_WRAP, D3D12_SHADER_VISIBILITY_PIXEL, 0, 0);
    rootSignatureDesc.pStaticSamplers = &samplerDesc;
    rootSignatureDesc.NumStaticSamplers = 1;
    Microsoft::WRL::ComPtr<ID3DBlob> rootSigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to D3D12SerializeRootSignature");
        return;
    }

    hr = DX12::getInstance().getDevice()->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to CreateRootSignature");
        return;
    }
}

void LoadTexture::loadTexture(const wchar_t* textureName)
{
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImg = {};

    //! WIC経由で画像を読み込み（png, jpg, bmpなど対応）
    HRESULT hr = DirectX::LoadFromWICFile(textureName, DirectX::WIC_FLAGS_NONE, &metadata, scratchImg);
    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to load texture");
        return;
    }

    //! ヒープ設定
    D3D12_HEAP_PROPERTIES textureHeapprop = {};
    textureHeapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
    textureHeapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    textureHeapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    //! リソース作成
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Format = metadata.format;
    resDesc.Width = metadata.width;
    resDesc.Height = static_cast<UINT>(metadata.height);
    resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    resDesc.SampleDesc.Count = 1;
    resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto device = DX12::getInstance().getDevice();

    //! どのシェーダーで読み込みか伝える
    hr = device->CreateCommittedResource(
        &textureHeapprop,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,     //!< ここでピクセルシェーダーで設定しているのでそれに適応される
        nullptr,
        IID_PPV_ARGS(m_texture.GetAddressOf())
    );

    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to create texture resource");
        return;
    }

    //! データを書き込み
    const DirectX::Image* img = scratchImg.GetImage(0, 0, 0);
    hr = m_texture->WriteToSubresource(
        0,
        nullptr,
        img->pixels,
        static_cast<UINT>(img->rowPitch),
        static_cast<UINT>(img->slicePitch)
    );

    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to write texture data");
        return;
    }

    //! ディスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descHeapDesc.NumDescriptors = 2;
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hr = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&m_basicDescHeap));

    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to create descriptor heap");
        return;
    }

    //! シェーダーリソースビュー(SRV)作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = resDesc.MipLevels;

    auto handle = m_basicDescHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateShaderResourceView(m_texture.Get(), &srvDesc, handle);

    //! 定数バッファ作成
    ID3D12Resource* constBuff = nullptr;
    auto& cameraMatrix = Camera::Instance().getMatrix();
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    UINT cbSize = (sizeof(Matrix) + 255) & ~255; // 256 バイトアライン
    auto resourceDescCB = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescCB,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constBuff)
    );

    //! マップ作成
    Matrix* mapMatrix = nullptr;
    constBuff->Map(0, nullptr, (void**)&mapMatrix);
    *mapMatrix = cameraMatrix;
    constBuff->Unmap(0, nullptr);

    //! 定数バッファのデータ確保
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;

    // 次スロットへ移動して CBV 作成
    handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateConstantBufferView(&cbvDesc, handle);
}