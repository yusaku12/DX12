#pragma once

#include "pmxload.h"

//=====================================================
// PmxActor クラス
//=====================================================
class PmxActor
{
public:

    PmxActor();
    ~PmxActor();

    //! モデル読み込み
    bool loadPmxModel(const std::wstring& filePath);

    //! 更新処理
    void update();

    //! 描画
    void draw() const;

private:

    //! モデルの頂点構造体
    struct Vertex
    {
        Vector3 position = {};
        Vector3 normal = {};
        Vector2 uv = {};
    };

    //! 頂点情報をコピー
    void loadVertexData(const std::vector<PmxLoad::PMXVertex>& vertex);

    Vertex* m_mapVertex;                  //!< 頂点構造体のポインタ
    std::vector<Vertex>m_containerVector; //!< データ格納コンテナ
    Microsoft::WRL::ComPtr<ID3D12Resource>m_vertexBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource>m_IndexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
};