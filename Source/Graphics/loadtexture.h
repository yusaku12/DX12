#pragma once

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// LoadTexture クラス
//=====================================================
class LoadTexture
{
public:

    LoadTexture(const wchar_t* textureName);
    ~LoadTexture() {};

    //! ディスクリプタ ヒープ取得
    ID3D12DescriptorHeap* getDescriptorHeapTexture() const { return m_basicDescHeap.Get(); }

private:

    //! テクスチャ読み込み
    void loadTexture(const wchar_t* textureName);

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_basicDescHeap;
};