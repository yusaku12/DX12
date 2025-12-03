#pragma once

#include "pmxload.h"

//=====================================================
// PmxActor クラス
//=====================================================
class PmxActor
{
public:

    explicit PmxActor(const std::wstring& filePath);
    ~PmxActor() {};

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
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_materialBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW  m_indexBufferView = {};
    char* m_mappedMaterial = nullptr;
};