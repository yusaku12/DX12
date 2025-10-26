#pragma once

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// LoadTexture �N���X
//=====================================================
class LoadTexture
{
public:

    explicit LoadTexture(const wchar_t* textureName);

    //! �e�N�X�`����K�p
    void applyTexture();

    //! ���[�g�V�O�l�`���ݒ�
    void setRootSignature();

    //! �f�B�X�N���v�^�q�[�v���擾
    ID3D12DescriptorHeap* getDescriptorHeap() const { return m_basicDescHeap.Get(); }

    //! �e�N�X�`���̃��\�[�X�擾
    ID3D12Resource* getResource() const { return m_texture.Get(); }

    //! ���[�g�V�O�l�`���擾
    ID3D12RootSignature* getRootSignature()const { return m_rootSignature.Get(); }

private:

    //! �e�N�X�`���ǂݍ���
    void loadTexture(const wchar_t* textureName);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_basicDescHeap;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
};