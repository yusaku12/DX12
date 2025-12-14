#pragma once

#include "PmxLoad.h"
#include "Graphics\CreateVertexData.h"
#include "Graphics\CreateIndexData.h"
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

    //! モデル読み込み
    bool loadPmxModel(const std::wstring& filePath);

    //! 頂点情報をコピー
    void loadVertexData(const std::vector<PmxLoad::PMXVertex>& vertex);

    //! マテリアル情報コピー
    bool loadMaterialData();

    PmxLoad::PMXFileData m_pmxFileData;     //!< データ構造体
    std::vector<Vertex> m_containerVector;  //!< データ格納コンテナ
    std::unique_ptr<CreateVertexData>m_vertexData;
    std::unique_ptr<CreateIndexData>m_indexData;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_materialBuffer = nullptr;
    char* m_mappedMaterial = nullptr;
};