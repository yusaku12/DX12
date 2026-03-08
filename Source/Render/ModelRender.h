#pragma once

#include "Model\ModelData.h"
#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"
#include "Graphics\ConstantBuffer.h"

class TransformComponent;

//----------------------------------------------------------------------------
// ModelRender — 統一モデルレンダラー
// PMX / FBX / .mdl いずれからでも生成可能
//----------------------------------------------------------------------------
class ModelRender
{
public:

    //! .mdl ファイルからロード
    explicit ModelRender(const std::string& mdlPath);

    //! ModelData から直接生成（インポート直後など）
    explicit ModelRender(ModelData&& data);

    ~ModelRender() = default;

    //! 描画
    void render();

    //! 描画（指定コマンドリストに対して）
    void render(ID3D12GraphicsCommandList* cmd);

    //! Transform 紐付け
    void setTransform(TransformComponent* tf) { m_transform = tf; }

    //! モデルデータ取得（読み取り専用）
    const ModelData& getModelData() const { return m_modelData; }

    //! モデルデータ取得（編集用）
    ModelData& getModelData() { return m_modelData; }

    //! .mdl として保存
    bool saveToMdl(const std::string& path) const { return m_modelData.saveToMdl(path); }

private:

    //! マテリアルあたりの最大テクスチャ数
    enum class TextureType : UINT
    {
        Diffuse,
        Normal,
        Toon,
        Max
    };

    //! マテリアルCBV構造体
    struct MaterialCB
    {
        Vector4 diffuse;
        Vector3 specular;
        float   specularPower;
        Vector3 ambient;
        float   _pad0;
        Vector3 emissive;
        float   _pad1;
    };

    //! サブセット構造体
    struct Subset
    {
        UINT indexCount = 0;
        UINT startIndex = 0;
        UINT materialIndex = 0;
        bool visible = true;
        std::array<int, static_cast<int>(TextureType::Max)> textureIndices{};
        UINT descriptorBase = UINT_MAX;
    };

    //! モデル行列CBV
    struct ModelCB
    {
        Matrix world = {};
    };

    //! メッシュ描画データ
    struct MeshDrawData
    {
        std::unique_ptr<VertexBuffer<ModelVertex>> vertexBuffer;
        std::unique_ptr<IndexBuffer<uint32_t>>     indexBuffer;
        std::vector<Subset>                        subsets;
    };

    //! GPU リソース構築
    void buildGPUResources();

    //! マテリアルCBV構築
    void createMaterialCBV();

    //! テクスチャ読み込み
    void createTextures();

    //! PSO 構築
    void createPSO();

    //! Descriptor 再構築
    void rebuildSubsetDescriptors(Subset& subset);

    ModelData                                 m_modelData;
    std::vector<MeshDrawData>                 m_meshes;
    std::unique_ptr<ConstantBuffer<MaterialCB>> m_materialCB;
    std::unique_ptr<ConstantBuffer<ModelCB>>  m_modelCB;
    size_t                                    m_psoKey = 0;
    std::vector<LoadTexture*>                 m_textures;
    std::vector<std::wstring>                 m_texturePaths;

    TransformComponent* m_transform = nullptr;
};