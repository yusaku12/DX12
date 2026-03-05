#pragma once

#include "Model\FBXLoad.h"
#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"
#include "Graphics\ConstantBuffer.h"

class TransformComponent; // 前方宣言

//----------------------------------------------------------------------------
// FBXRender
//----------------------------------------------------------------------------
class FBXRender
{
public:

    explicit FBXRender(const std::string& filePath);
    ~FBXRender() {};

    //! 描画
    void render();

    //! 描画（指定コマンドリストに対して）
    void render(ID3D12GraphicsCommandList* cmd);

    //! デバック描画
    void debugRender();

    //! このレンダラに TransformComponent を紐付ける（nullptr で解除）
    void setTransform(TransformComponent* tf) { m_transform = tf; }

private:

    //! マテリアルあたりの最大テクスチャ数
    enum class TextureType : UINT
    {
        Diffuse,
        Normal,
        Max
    };

    //! 頂点構造体（GPU送信用）
    struct Vertex
    {
        Vector3 position;
        Vector3 normal;
        Vector4 tangent;
        Vector2 uv;
    };

    //! マテリアルCBV構造体
    struct Material
    {
        Vector4 diffuse;
        Vector3 specular;
        Vector3 ambient;
    };

    //! サブセット構造体
    struct Subset
    {
        UINT indexCount;
        UINT startIndex;
        UINT materialIndex;
        bool visible = true;
        std::array<int, static_cast<int>(TextureType::Max)> textureIndices{};
        UINT descriptorBase = UINT_MAX;
    };

    //! モデル構造体（ワールド行列）
    struct ModelCB
    {
        Matrix world = {};
    };

    //! メッシュ描画データ
    struct MeshData
    {
        std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer;
        std::unique_ptr<IndexBuffer<uint32_t>> indexBuffer;
        std::vector<Subset> subsets;
    };

    //! メッシュ描画データの作成
    void createMeshData();

    //! マテリアルCBVの作成
    void createMaterialCBV();

    //! テクスチャ読み込み
    void createTextures();

    //! PSOの作成
    void createPSO();

    //! サブセットの Descriptor を再構築
    void rebuildSubsetDescriptors(Subset& subset);

    //! 設定の読み込み
    void loadSetting();

    //! 設定の保存
    void saveSetting();

    FBXLoad::Model m_model;
    std::vector<MeshData> m_meshes;
    std::unique_ptr<ConstantBuffer<Material>> m_materialCB;
    std::unique_ptr<ConstantBuffer<ModelCB>> m_modelCB;
    size_t m_psoKey = 0;
    std::vector<LoadTexture*> m_textures;
    std::vector<std::wstring> m_texturePaths;
    std::string m_settingPath;

    TransformComponent* m_transform = nullptr;
};