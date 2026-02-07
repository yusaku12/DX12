#pragma once

#include "DescriptorHeapManager.h"
#include "UploadBuffer.h"
#include <DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//=====================================================
// テクスチャ読み込みクラス
//=====================================================
class LoadTexture
{
public:

    explicit LoadTexture(const std::wstring& filePath);

    //! テクスチャリソース取得
    ID3D12Resource* getResource() const { return m_texture.Get(); }

    //! GPUハンドル取得（描画時用）
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const { return DescriptorHeapManager::Instance().getGPUHandle(m_srvIndex); }

    //! SRVインデックス取得
    UINT getSRVIndex() const { return m_srvIndex; }

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
    UINT m_srvIndex = 0;
    std::unique_ptr<UploadBuffer> m_upload;
};