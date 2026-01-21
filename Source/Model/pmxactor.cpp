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

    m_vertexBuffer->bind();
    m_indexBuffer->bind();
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
    m_vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(m_containerVector);

    //! インデックスバッファ作成
    m_indexBuffer = std::make_unique<IndexBuffer<PmxLoad::PMXFace>>(m_pmxFileData.faces, DXGI_FORMAT_R32_UINT);

    //! モデル行列用定数バッファ作成
    m_modelMatrixCB = ConstantBuffer<ModelMatrix>();

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
    const UINT materialCount = static_cast<UINT>(m_pmxFileData.materials.size());
    if (materialCount == 0)
        return true;

    //! Material CB（配列）
    m_materialCB = ConstantBuffer<Material>(materialCount);
    m_materialSRVs.resize(materialCount);

    //! RootSignature にパラメータ追加
    RootSignatureManager::Instance().addParameterTo(RootSignatureType::Standard, m_materialCB.createRootParameter(2));

    auto& texMgr = TextureManager::Instance();

    for (UINT i = 0; i < materialCount; ++i)
    {
        const auto& srcMat = m_pmxFileData.materials[i];

        //! Material 定数
        Material mat{};
        mat.diffuse = srcMat.diffuse;
        mat.specular = srcMat.specular;
        mat.specularPower = srcMat.specularPower;
        mat.ambient = srcMat.ambient;
        m_materialCB.update(i, mat);

        //! Diffuse Texture
        if (!srcMat.texturePath.empty())
        {
            auto tex = texMgr.load(srcMat.texturePath);
            m_materialSRVs[i].diffuse = tex->getSRVIndex();
        }
        else
        {
            m_materialSRVs[i].diffuse = texMgr.getWhiteTextureSRVIndex();
        }

        //! Toon Texture
        if (!srcMat.toonTexturePath.empty())
        {
            auto tex = texMgr.load(srcMat.toonTexturePath);
            m_materialSRVs[i].toon = tex->getSRVIndex();
        }
        else
        {
            m_materialSRVs[i].toon = texMgr.getWhiteTextureSRVIndex();
        }

        //! Sphere Texture
        if (!srcMat.sphereTexturePath.empty())
        {
            auto tex = texMgr.load(srcMat.sphereTexturePath);
            m_materialSRVs[i].sphere = tex->getSRVIndex();
        }
        else
        {
            m_materialSRVs[i].sphere = texMgr.getWhiteTextureSRVIndex();
        }
    }

    return true;
}