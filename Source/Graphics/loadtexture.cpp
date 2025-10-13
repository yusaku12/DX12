#include "pch.h"
#include "loadtexture.h"

void loadTexture(const wchar_t* textureName)
{
    DirectX::TexMetadata metadata = {};
    DirectX::ScratchImage scratchImg = {};

    //! �e�N�X�`����ǂݍ���
    HRESULT hr = DirectX::LoadFromWICFile(textureName, DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImg);

    //! �e�N�X�`���o�b�t�@�쐬
    D3D12_HEAP_PROPERTIES textureHeapprop = {};
    textureHeapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
    textureHeapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    textureHeapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    textureHeapprop.CreationNodeMask = 0;
    textureHeapprop.VisibleNodeMask = 0;

    //! ���\�[�X���쐬
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Format = metadata.format;
    resDesc.Width = metadata.width;
    resDesc.Height = metadata.height;
    resDesc.DepthOrArraySize = metadata.arraySize;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;
    resDesc.MipLevels = metadata.mipLevels;
    resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    //! ���\�[�X���g�p
    ID3D12Resource* texbuff = nullptr;
    hr = DX12::getInstance().getDevice()->CreateCommittedResource(
        &textureHeapprop,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,       //!< �����͈����V�F�[�_�ɂ���ĕύX���s��
        nullptr,
        IID_PPV_ARGS(&texbuff)
    );

    //! �o�b�t�@�ɉ摜�f�[�^����������
    const DirectX::Image* img = scratchImg.GetImage(0, 0, 0);
    hr = texbuff->WriteToSubresource(
        0,                          //!< �~�b�v�}�b�v�ݒ�
        nullptr,
        img->pixels,
        img->rowPitch,
        img->slicePitch);

    //! �f�B�X�N���v�^ �q�[�v�쐬
    ID3D12DescriptorHeap* basicDescHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descHeapDesc.NodeMask = 0;
    descHeapDesc.NumDescriptors = 1;
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hr = DX12::getInstance().getDevice()->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

    //! �V�F�[�_�[���\�[�X�r���[�쐬
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;    //2�����e�N�X�`�����g�p����̂ł����OK
    srvDesc.Texture2D.MipLevels = 1;
    auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
    DX12::getInstance().getDevice()->CreateShaderResourceView(texbuff, &srvDesc, basicHeapHandle);

    //! ���[�g�p�����[�^
    D3D12_DESCRIPTOR_RANGE textureDescriptorRange = {};
    textureDescriptorRange.NumDescriptors = 1;
    textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureDescriptorRange.BaseShaderRegister = 0;
    textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //! ���[�g�p�����[�^�쐬
    D3D12_ROOT_PARAMETER rootparam[1] = {};
    rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootparam[0].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
    rootparam[0].DescriptorTable.NumDescriptorRanges = 1;
}