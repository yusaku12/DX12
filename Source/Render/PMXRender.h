#pragma once

#include "Model\PmxLoad.h"
#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"
#include "Graphics\ConstantBuffer.h"
#include "Graphics\PSOCreator.h"

//----------------------------------------------------------------------------
// PMXRender
//----------------------------------------------------------------------------
class PMXRender
{
public:

    explicit PMXRender(const std::wstring& filePath);
    ~PMXRender() {};

    //! 描画
    void render();

    //! デバック描画
    void debugRender();

private:

    //! 頂点構造体
    struct Vertex
    {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;

        //uint32_t boneIndex[4];
        //float    boneWeight[4];
    };

    //! マテリアルCBV構造体
    struct Material
    {
        Vector4 diffuse;
        Vector3 specular;
        //float specularPower;
        Vector3 ambient;
    };

    //! サブセット構造体
    struct Subset
    {
        UINT indexCount;
        UINT startIndex;
        UINT materialIndex;
        std::vector<int> textureIndices;
        bool visible = true;
    };

    //! モデル構造体
    struct Model
    {
        Matrix world = {};
    };

    //! 頂点バッファの作成
    void createVertexBuffer();

    //! インデックスバッファの作成
    void createIndexBuffer();

    //! マテリアルCBVの作成
    void createMaterialCBV();

    //! サブセットの作成
    void createSubsets();

    //! テクスチャ読み込み
    void createTextures();

    //! PSOの作成
    void createPSO();

    // ! 設定の読み込み
    void loadSetting();

    //! 設定の保存
    void saveSetting();

    PmxLoad::PMXFileData m_pmxFileData;
    std::unique_ptr<PmxLoad>m_pmxLoad;
    std::unique_ptr<VertexBuffer<Vertex>> m_vertexBuffer;
    std::unique_ptr<IndexBuffer<unsigned int>> m_indexBuffer;
    std::unique_ptr<ConstantBuffer<Material>> m_materialCB;
    std::unique_ptr<ConstantBuffer<Model>> m_modelCB;
    std::unique_ptr<PSOCreator>m_psoCreator;
    std::vector<LoadTexture*> m_textures;
    std::vector<Subset> m_subsets;
    std::vector<std::wstring> m_texturePaths;
    std::wstring m_settingPath;
};