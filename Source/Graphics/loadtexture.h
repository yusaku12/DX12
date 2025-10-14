#pragma once

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// LoadTexture �N���X
//=====================================================
class LoadTexture
{
public:

    LoadTexture(const wchar_t* textureName);
    ~LoadTexture() {};

    //! �f�B�X�N���v�^ �q�[�v�擾
    ID3D12DescriptorHeap* getDescriptorHeapTexture() const { return m_basicDescHeap.Get(); }

private:

    //! �e�N�X�`���ǂݍ���
    void loadTexture(const wchar_t* textureName);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_basicDescHeap;
};