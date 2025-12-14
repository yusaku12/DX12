#pragma once

//=====================================================
// インデックスデータ作成クラス
//=====================================================
class CreateIndexData
{
public:

    CreateIndexData() {};
    ~CreateIndexData() {};

    //! インデックスリソース設定
    template<typename T>
    void settingIndexResource(const std::vector<T>& data, DXGI_FORMAT format)
    {
        auto device = DX12::Instance().getDevice();

        const UINT bufferSize = UINT(sizeof(T) * data.size());

        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_indexBuffer.GetAddressOf())
        );
        if (!SUCCEEDED(hr)) return;

        //! Map
        void* mapped = nullptr;
        hr = m_indexBuffer->Map(0, nullptr, &mapped);
        if (!SUCCEEDED(hr)) return;

        //! Copy
        memcpy(mapped, data.data(), bufferSize);

        //! Unmap
        m_indexBuffer->Unmap(0, nullptr);

        //! バッファビュー設定
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = bufferSize;
        m_indexBufferView.Format = format;
    }

    //! インデックスバッファ設定
    void bindIndexBuffer();

private:

    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
};