#pragma once

#include "PmxLoad.h"
#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"
#include "Graphics\ConstantBuffer.h"

//=====================================================
// PMXモデルを扱うクラス
//=====================================================
class PmxActor
{
public:

    explicit PmxActor(const std::wstring& filePath);
    ~PmxActor() {};

    //! 描画
    void render() const;

private:

    //! モデル行列
    struct ModelMatrix
    {
        Matrix world = Matrix::Identity;
    };

    //! モデルの頂点構造体
    struct Vertex
    {
        Vector3 position = {};
        Vector3 normal = {};
        Vector2 uv = {};
    };

    //! マテリアル構造体
    struct Material
    {
        Vector4 diffuse = {};
        Vector3 specular = {};
        float specularPower = {};
        Vector3 ambient = {};
    };

    //! マテリアル用SRV構造体
    struct MaterialSRV
    {
        UINT diffuse;
        UINT toon;
        UINT sphere;
    };

    //! モデル読み込み
    bool loadPmxModel(const std::wstring& filePath);

    //! 頂点情報をコピー
    void loadVertexData(const std::vector<PmxLoad::PMXVertex>& vertex);

    //! マテリアル情報コピー
    bool loadMaterialData();

    PmxLoad::PMXFileData m_pmxFileData;     //!< データ構造体
    std::vector<Vertex> m_containerVector;  //!< データ格納コンテナ
    std::vector<MaterialSRV> m_materialSRVs; //!< マテリアル用SRVコンテナ
    std::unique_ptr<VertexBuffer<Vertex>> m_vertexBuffer;         //!< 頂点バッファ
    std::unique_ptr<IndexBuffer<PmxLoad::PMXFace>> m_indexBuffer; //!< インデックスバッファ
    ConstantBuffer<ModelMatrix> m_modelMatrixCB; //!< モデル行列用定数バッファ
    ConstantBuffer<Material> m_materialCB;    //!< マテリアル用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> m_materialBuffer = nullptr;
    char* m_mappedMaterial = nullptr;
};