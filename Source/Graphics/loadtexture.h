#pragma once

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// LoadTexture クラス
//=====================================================
class LoadTexture
{
public:

    explicit LoadTexture(const wchar_t* textureName);

    //! テクスチャを適用
    void applyTexture();

    //! ルートシグネチャ設定
    void setRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature);

    //! ディスクリプタヒープを取得
    ID3D12DescriptorHeap* getDescriptorHeap() const { return m_basicDescHeap.Get(); }

    //! テクスチャのリソース取得
    ID3D12Resource* getResource() const { return m_texture.Get(); }

private:

    //! テクスチャ読み込み
    void loadTexture(const wchar_t* textureName);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_basicDescHeap;
};