#pragma once

//=====================================================
// GraphicsSetting クラス
//=====================================================
class GraphicsSetting
{
public:

    GraphicsSetting();
    ~GraphicsSetting();

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

    //! コンスタントバッファ設定
    template<typename T>
    void settingConstantBuffer(const T& data)
    {
        auto device = DX12::Instance().getDevice();
        auto cmd = DX12::Instance().getGraphicsCommandList();

        //! 256バイトアライン
        const UINT cbSize = (sizeof(T) + 255) & ~255;

        //! まだ CB が作られていなければ作る
        if (!m_constantBuffer)
        {
            CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer)
            );
            if (!SUCCEEDED(hr)) return;

            //! Map
            hr = m_constantBuffer->Map(0, nullptr, &m_mappedCB);
            if (!SUCCEEDED(hr)) { m_mappedCB = nullptr; return; }

            //! CBV を作成（まだハンドルを持っていなければ割当て）
            if (m_cbvHandleCPU.ptr == 0)
            {
                m_cbvHandleCPU = DX12::Instance().allocateCbvSrvHandle();
                m_cbvHandleGPU = DX12::Instance().getGpuHandle(m_cbvHandleCPU);

                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
                cbvDesc.SizeInBytes = cbSize;

                device->CreateConstantBufferView(&cbvDesc, m_cbvHandleCPU);
            }
        }
    }

    //! コンスタントバッファを更新
    template<typename T>
    void updateConstantBuffer(const T& data)
    {
        memcpy(m_mappedCB, &data, sizeof(T));
    }

    //! コンスタントバッファをバインド
    void bindConstantBuffer(UINT rootParameterIndex, bool rootIsDescriptorTable = true);

    //! メッシュを描画するためのバッファ（VB・IB）を設定
    void setMeshBuffers(D3D12_PRIMITIVE_TOPOLOGY topology);

private:

    //! 個別に保持する
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};

    //! ConstantBuffer 関連
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    void* m_mappedCB;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cbvHandleCPU = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE m_cbvHandleGPU = { 0 };
};