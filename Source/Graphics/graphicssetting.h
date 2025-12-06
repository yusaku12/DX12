#pragma once

//=====================================================
// GraphicsSetting クラス
//=====================================================
class GraphicsSetting
{
public:

    GraphicsSetting();
    ~GraphicsSetting() {}

    //! バーテックスリソース設定
    template<typename T>
    void setteingVertexResource(const std::vector<T>& data, UINT sizeInBytes, UINT strideInBytes)
    {
        auto device = DX12::Instance().getDevice();

        const UINT bufferSize = UINT(sizeof(T) * data.size());

        m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        m_resourceDesc.Width = bufferSize;
        m_resourceDesc.Height = 1;
        m_resourceDesc.DepthOrArraySize = 1;
        m_resourceDesc.MipLevels = 1;
        m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        m_resourceDesc.SampleDesc.Count = 1;
        m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &m_resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexResource)
        );
        if (!SUCCEEDED(hr))return;

        //! Map
        void* mapped = nullptr;
        hr = vertexResource->Map(0, nullptr, &mapped);
        if (!SUCCEEDED(hr))return;

        //! Copy
        memcpy(mapped, data.data(), bufferSize);

        hr = vertexResource->Unmap(0, nullptr);
        if (!SUCCEEDED(hr))return;

        //! バッファビュー設定
        m_vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
        m_vertexBufferView.SizeInBytes = sizeInBytes;
        m_vertexBufferView.StrideInBytes = strideInBytes;
    }

    //! インデックスリソース設定
    template<typename T>
    void setteingIndexResource(const std::vector<T>& data, UINT sizeInBytes, DXGI_FORMAT format)
    {
        auto device = DX12::Instance().getDevice();

        const UINT bufferSize = UINT(sizeof(T) * data.size());

        m_resourceDesc.Width = bufferSize;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &m_resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexResource)
        );

        //! Map
        void* mapped = nullptr;
        hr = indexResource->Map(0, nullptr, &mapped);
        if (!SUCCEEDED(hr))return;

        //! Copy
        memcpy(mapped, data.data(), bufferSize);

        hr = indexResource->Unmap(0, nullptr);
        if (!SUCCEEDED(hr))return;

        //! バッファビュー設定
        m_indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = sizeInBytes;
        m_indexBufferView.Format = format;
    }

    //! コンスタントバッファ設定
    template<typename T>
    void settingConstantBuffer(const T& data)
    {
        auto device = DX12::Instance().getDevice();

        // 256バイトアライン
        UINT cbSize = (sizeof(T) + 255) & ~255;

        auto resourceDescCB = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDescCB,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constantBuffer)
        );
        if (!SUCCEEDED(hr))return;

        //! Map
        void* mapped = nullptr;
        hr = constantBuffer->Map(0, nullptr, &mapped);
        if (!SUCCEEDED(hr))return;

        //! Copy
        memcpy(mapped, &data, sizeof(T));

        hr = constantBuffer->Unmap(0, nullptr);
        if (!SUCCEEDED(hr))return;

        //! 定数バッファのデータ確保
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = cbSize;

        //! ハンドル割り当て
        m_cbvHandle = DX12::Instance().allocateCbvSrvHandle();

        //! GPU に登録
        device->CreateConstantBufferView(&cbvDesc, m_cbvHandle);
    }

    //! Root Signature のスロットにバインド
    void bindRootSignature(UINT rootParameterIndex);

    //! メッシュを描画するためのバッファ（VB・IB）を設定
    void setMeshBuffers(D3D12_PRIMITIVE_TOPOLOGY topology);

private:

    D3D12_RESOURCE_DESC m_resourceDesc = {};
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_cbvHandle = {};
};