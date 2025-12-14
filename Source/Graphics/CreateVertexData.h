#pragma once

//=====================================================
// バーテックスデータ作成クラス
//=====================================================
class CreateVertexData
{
public:

    CreateVertexData() {};
    ~CreateVertexData() {};

    //! バーテックスリソース設定
    template<typename T>
    void settingVertexResource(const std::vector<T>& data, UINT strideInBytes)
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
            IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())
        );
        if (!SUCCEEDED(hr)) return;

        //! Map
        void* mapped = nullptr;
        hr = m_vertexBuffer->Map(0, nullptr, &mapped);
        if (!SUCCEEDED(hr)) return;

        //! Copy
        memcpy(mapped, data.data(), bufferSize);

        //! Unmap
        m_vertexBuffer->Unmap(0, nullptr);

        //! バッファビュー設定
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = bufferSize;
        m_vertexBufferView.StrideInBytes = strideInBytes;
    }

    //! バーテックスバッファ設定
    void bindVertexBuffer();

private:

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
};