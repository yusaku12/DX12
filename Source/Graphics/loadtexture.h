#pragma once

#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// テクスチャ読み込みクラス
//=====================================================
class LoadTexture
{
public:

    explicit LoadTexture(const std::wstring& filePath);

    //! シェーダリソースビュー用デスクリプタヒープ取得
    ID3D12DescriptorHeap* getSrvHeap() const { return m_srvHeap.Get(); }

    //! テクスチャリソース取得
    ID3D12Resource* getResource() const { return m_texture.Get(); }

    //! 読み込み成功しているか
    bool isValid() const { return m_isValid; }

private:

    //! ローダー関数型定義
    using LoaderFunc = std::function<HRESULT(const std::wstring&, DirectX::TexMetadata*, DirectX::ScratchImage&)>;

    //! ローダーテーブル初期化
    void initLoaderTable();

    //! ファイルからテクスチャ読み込み
    bool loadFromFile(const std::wstring& filePath);

    //! テクスチャリソース作成
    void createTextureResource(const DirectX::TexMetadata& meta, const DirectX::ScratchImage& img);

    bool m_isValid = false;  //!< 読み込み成功フラグ
    std::unordered_map<std::wstring, LoaderFunc> m_loaderTable; //!< ローダーテーブル
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture; //!< テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap; //!< シェーダリソースビュー用デスクリプタヒープ
};