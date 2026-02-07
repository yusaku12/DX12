#pragma once

//--------------------------------------------------------------
// データアップロード用バッファ
//--------------------------------------------------------------
class UploadBuffer
{
public:

    explicit UploadBuffer(UINT64 size = 0);
    ~UploadBuffer() {};

    //! リソース取得
    ID3D12Resource* getResource() const { return m_resource.Get(); }

private:

    //! 作成
    void create(UINT64 size);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
};