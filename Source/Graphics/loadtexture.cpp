#include "pch.h"
#include "loadtexture.h"

LoadTexture::LoadTexture(const wchar_t* textureName)
{
    loadTexture(textureName);
}

void LoadTexture::applyTexture()
{
    //!@todo �Ȍ�C���Č�
    auto commandList = DX12::getInstance().getGraphicsCommandList();
    ID3D12DescriptorHeap* heaps[] = { m_basicDescHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetGraphicsRootDescriptorTable(0, m_basicDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void LoadTexture::setRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature)
{
    //!@todo ����C���Č�

    //! �e�N�X�`���̃��[�g�p�����[�^
    D3D12_DESCRIPTOR_RANGE textureDescriptorRange = {};
    textureDescriptorRange.NumDescriptors = 1;
    textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;     //!< SRV�Ȃ̂ł���
    textureDescriptorRange.BaseShaderRegister = 0;
    textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //! ���[�g�p�����[�^�쐬
    D3D12_ROOT_PARAMETER rootparam[1] = {};
    rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;           //!< �s�N�Z���V�F�[�_�Ŏg�p����̂�
    rootparam[0].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
    rootparam[0].DescriptorTable.NumDescriptorRanges = 1;

    //! ���[�g�V�O�l�`��(�ǂ̃V�F�[�_���\�[�X���g�p����̂��A�ǂ̃T���v���[���g�p����̂���ݒ肷�����)
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;  //!< ���_��񂪑��݂���
    rootSignatureDesc.pParameters = rootparam;
    rootSignatureDesc.NumParameters = 1;
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

    //! WIC�o�R�ŉ摜��ǂݍ��݁ipng, jpg, bmp�ȂǑΉ��j
    HRESULT hr = DirectX::LoadFromWICFile(textureName, DirectX::WIC_FLAGS_NONE, &metadata, scratchImg);
    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to load texture");
        return;
    }

    //! �q�[�v�ݒ�
    D3D12_HEAP_PROPERTIES textureHeapprop = {};
    textureHeapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
    textureHeapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    textureHeapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    //! ���\�[�X�쐬
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

    //! �ǂ̃V�F�[�_�[�œǂݍ��݂��`����
    hr = device->CreateCommittedResource(
        &textureHeapprop,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,     //!< �����Ńs�N�Z���V�F�[�_�[�Őݒ肵�Ă���̂ł���ɓK�������
        nullptr,
        IID_PPV_ARGS(m_texture.GetAddressOf())
    );

    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to create texture resource");
        return;
    }

    //! �f�[�^����������
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

    //! �f�B�X�N���v�^�q�[�v�쐬
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descHeapDesc.NumDescriptors = 1;
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hr = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_basicDescHeap.GetAddressOf()));

    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to create descriptor heap");
        return;
    }

    //! �V�F�[�_�[���\�[�X�r���[(SRV)�쐬
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    auto handle = m_basicDescHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateShaderResourceView(m_texture.Get(), &srvDesc, handle);
}