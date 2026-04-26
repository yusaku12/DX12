#pragma once

//=====================================================
//! IBL 用テクスチャ管理
//=====================================================
class IBLManager
{
public:

    //! シングルトンインスタンス取得
    static IBLManager& Instance()
    {
        static IBLManager instance;
        return instance;
    }

    //! 初期化
    void initialize();

    //! 環境キューブマップをセット（Irradiance/Prefilter 両方に使用）
    void setEnvironmentCubemap(const std::wstring& path);

    //! Irradiance キューブマップをセット
    void setIrradianceCubemap(const std::wstring& path);

    //! Prefilter キューブマップをセット
    void setPrefilteredCubemap(const std::wstring& path);

    //! IBL 用ディスクリプタテーブル取得（t2～t3）
    D3D12_GPU_DESCRIPTOR_HANDLE getDescriptorHandle() const;

private:

    IBLManager() = default;

    //! ディスクリプタテーブル再構築
    void rebuildDescriptorTable();

    //! IBL 用のパス生成
    std::wstring buildIblPath(const std::wstring& basePath, const wchar_t* suffix) const;

    LoadTexture* m_irradiance = nullptr;
    LoadTexture* m_prefilter = nullptr;
    std::wstring m_irradiancePath;
    std::wstring m_prefilterPath;
    UINT m_descriptorBase = UINT_MAX;
};