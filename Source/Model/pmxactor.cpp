#include "pch.h"
#include "pmxactor.h"

bool PmxActor::loadPmxModel(const std::wstring& filePath)
{
    //auto device = DX12::getInstance().getDevice();

    ////! 頂点バッファ作成
    //D3D12_HEAP_PROPERTIES heapprop = {};
    //heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
    //heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    //heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ////! メモリ確保
    //D3D12_RESOURCE_DESC resdesc = {};
    //size_t vertexSize = sizeof(m_mapVertex);
    //resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    //resdesc.Width = m_containerVector.size() * vertexSize;
    //resdesc.Height = 1;
    //resdesc.DepthOrArraySize = 1;
    //resdesc.MipLevels = 1;
    //resdesc.Format = DXGI_FORMAT_UNKNOWN;
    //resdesc.SampleDesc.Count = 1;
    //resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    //resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ////! リソース作成
    //auto result = device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf()));
    //if (FAILED(result))
    //{
    //    assert(SUCCEEDED(result));
    //    return result;
    //}

    ////! データをマップ
    //result = m_vertexBuffer->Map(0, nullptr, (void**)&m_mapVertex);
    //if (FAILED(result))
    //{
    //    assert(SUCCEEDED(result));
    //    return result;
    //}

    ////! マップ
    //std::copy(std::begin(m_containerVector), std::end(m_containerVector), m_mapVertex);
    //m_vertexBuffer->Unmap(0, nullptr);

    ////! データ設定
    //m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    //m_vertexBufferView.SizeInBytes = vertexSize * mVertexCount;
    //m_vertexBufferView.StrideInBytes = vertexSize;

    ////! インデックスバッファのリソース作成
    //resdesc.Width = sizeof(unsigned int) * mIndexCount;
    //result = device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_IndexBuffer.ReleaseAndGetAddressOf()));
    //if (FAILED(result))
    //{
    //    assert(SUCCEEDED(result));
    //    return result;
    //}

    return true;
}

void PmxActor::loadVertexData(const std::vector<PmxLoad::PMXVertex>& vertex)
{
    m_containerVector.resize(vertex.size());

    for (int index = 0; index < vertex.size(); ++index)
    {
        const PmxLoad::PMXVertex& currentPmxVertex = vertex[index];
        Vertex& currentUploadVertex = m_containerVector[index];

        currentUploadVertex.position = currentPmxVertex.position;
        currentUploadVertex.normal = currentPmxVertex.normal;
        currentUploadVertex.uv = currentPmxVertex.uv;
    }
}