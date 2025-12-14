#include "pch.h"
#include "PmxActor.h"

PmxActor::PmxActor(const std::wstring& filePath)
{
    //! PMXモデル読み込み
    loadPmxModel(filePath);
}

void PmxActor::render() const
{
    auto cmd = DX12::Instance().getGraphicsCommandList();

    m_vertexData->bindVertexBuffer();
    m_indexData->bindIndexBuffer();
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool PmxActor::loadPmxModel(const std::wstring& filePath)
{
    //! PMXファイル読み込み
    std::unique_ptr<PmxLoad>pmxLoad = std::make_unique<PmxLoad>(filePath, m_pmxFileData);
    if (!pmxLoad)return false;

    //! 頂点情報をコピー
    loadVertexData(m_pmxFileData.vertices);

    //! 頂点バッファ作成
    m_vertexData = std::make_unique<CreateVertexData>();
    m_vertexData->settingVertexResource(m_containerVector, sizeof(Vertex));

    //! インデックスバッファ作成
    m_indexData = std::make_unique<CreateIndexData>();
    m_indexData->settingIndexResource(m_pmxFileData.faces, DXGI_FORMAT_R32_UINT);

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