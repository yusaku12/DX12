#include "pch.h"
#include "pmxactor.h"

PmxActor::PmxActor(const std::wstring& filePath)
{
    //! PMXモデル読み込み
    loadPmxModel(filePath);
}

bool PmxActor::loadPmxModel(const std::wstring& filePath)
{
    //! PMXファイル読み込み
    std::unique_ptr<PmxLoad>pmxLoad = std::make_unique<PmxLoad>(filePath, m_pmxFileData);
    if (!pmxLoad)return false;

    //! 頂点情報をコピー
    loadVertexData(m_pmxFileData.vertices);

    auto device = DX12::Instance().getDevice();

    //! 頂点バッファ作成
    D3D12_HEAP_PROPERTIES heapprop = {};
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    //! メモリ確保
    D3D12_RESOURCE_DESC resdesc = {};
    size_t vertexSize = sizeof(Vertex);
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = m_containerVector.size() * vertexSize;
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.Format = DXGI_FORMAT_UNKNOWN;
    resdesc.SampleDesc.Count = 1;
    resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    //! リソース作成
    Vertex* vertex = nullptr;
    HRESULT hr = device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf()));
    if (!SUCCEEDED(hr))return false;

    //! データをマップ
    hr = m_vertexBuffer->Map(0, nullptr, (void**)&vertex);
    if (!SUCCEEDED(hr))return false;

    //! コピー
    std::copy(std::begin(m_containerVector), std::end(m_containerVector), vertex);
    m_vertexBuffer->Unmap(0, nullptr);

    //! データ設定
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = vertexSize * m_pmxFileData.vertices.size();
    m_vertexBufferView.StrideInBytes = vertexSize;

    //! インデックスバッファのリソース作成
    unsigned int indexCount = m_pmxFileData.faces.size();
    resdesc.Width = sizeof(unsigned int) * indexCount;
    hr = device->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf()));
    if (!SUCCEEDED(hr))return false;

    //! データをマップ
    PmxLoad::PMXFace* pmxIndex = nullptr;
    hr = m_indexBuffer->Map(0, nullptr, (void**)&pmxIndex);
    if (!SUCCEEDED(hr))return false;

    //! コピー
    std::copy(std::begin(m_pmxFileData.faces), std::end(m_pmxFileData.faces), pmxIndex);
    m_indexBuffer->Unmap(0, nullptr);

    //! データ設定
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = sizeof(unsigned int) * indexCount;

    //! マテリアルデータ読み込み
    loadMaterialData();

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

bool PmxActor::loadMaterialData()
{
    auto device = DX12::Instance().getDevice();

    int materialBufferSize = sizeof(Material);
    materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

    //! リソース作成
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * m_pmxFileData.materials.size());
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_materialBuffer.ReleaseAndGetAddressOf())
    );
    if (!SUCCEEDED(hr))return false;

    //! マップ
    hr = m_materialBuffer->Map(0, nullptr, (void**)&m_mappedMaterial);
    if (!SUCCEEDED(hr))return false;

    char* mappedMaterialPtr = m_mappedMaterial;

    //! データーコピー
    for (const auto& material : m_pmxFileData.materials)
    {
        Material* uploadMat = reinterpret_cast<Material*>(mappedMaterialPtr);
        uploadMat->diffuse = material.diffuse;
        uploadMat->specular = material.specular;
        uploadMat->specularPower = material.specularPower;
        uploadMat->ambient = material.ambient;

        mappedMaterialPtr += materialBufferSize;
    }

    m_materialBuffer->Unmap(0, nullptr);

    return true;
}