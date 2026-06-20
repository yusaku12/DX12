#pragma once

#include "Graphics\VertexBuffer.h"
#include "Graphics\IndexBuffer.h"

//================================================
// テクスチャタイプ
//================================================
enum class TextureType : UINT
{
    Diffuse,
    Normal,
    Max
};

//------------------------------------------------
// モデルリソース
//------------------------------------------------
class ModelResource
{
private:

    //! 統計情報構造体
    struct Statistics
    {
        uint32_t totalVertices = 0;
        uint32_t totalIndices = 0;
        uint32_t totalTriangles = 0;
        uint32_t meshCount = 0;
        uint32_t materialCount = 0;
        uint32_t subMeshCount = 0;
        uint32_t drawCallCount = 0;   //!< 直近フレームのドローコール数
    };

public:

    explicit ModelResource() {};
    virtual ~ModelResource();

    // ボーン
    struct Bone
    {
        UINT64		id = {};
        std::string	name = {};
        int			parentIndex = {};
        Vector3	    scale = {};
        Vector4	    rotate = {};
        Vector3	    translate = {};
    };

    //! ノード情報
    struct NodeKeyData
    {
        Vector3	scale = {};
        Vector4	rotate = {};
        Vector3	translate = {};
    };

    //! キーフレーム
    struct Keyframe
    {
        float                    seconds = {};
        std::vector<NodeKeyData> nodeKeys;
    };

    //! アニメーション
    struct Animation
    {
        std::string			  name = {};
        float				  secondsLength = {};
        std::vector<Keyframe> keyframes;
    };

    //! 頂点
    struct Vertex
    {
        Vector3 position = {};
        Vector3 normal = {};
        Vector3 tangent = {};
        Vector2 uv = {};
        Vector4 boneWeights = Vector4(1, 0, 0, 0);
        XMUINT4 boneIndices = { 0, 0, 0, 0 };
    };

    //! サブメッシュ
    struct SubMesh
    {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
        uint32_t materialIndex = 0;
        std::array<int, static_cast<UINT>(TextureType::Max)> textureIndices{};
        UINT descriptorBase = UINT_MAX;
        bool visible = true;
    };

    //! メッシュ
    struct Mesh
    {
        std::string name = {};
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubMesh> subMeshes;
        int	nodeIndex = {};
        std::vector<int> nodeIndices;
        std::vector<Matrix> offsetTransforms;
        DirectX::XMFLOAT3 boundsMin = {};
        DirectX::XMFLOAT3 boundsMax = {};
        bool visible = true;
    };

    //! マテリアル
    struct Material
    {
        std::string name = {};
        std::array<std::string, static_cast<UINT>(TextureType::Max)>textureName;
        Vector4 diffuseColor = { 1,1,1,1 };
        float metallic = 1.0f;
        float roughness = 1.0f;
        float ao = 1.0f;
        bool visible = true;
    };

    //! モデル（最上位コンテナ）
    struct Model
    {
        std::vector<Bone> bones;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Animation> animations;

        //! スキンメッシュかどうか
        bool hasSkeleton() const { return !bones.empty(); }

        //! アニメーションを持つかどうか
        bool hasAnimation() const { return !animations.empty(); }
    };

    //! テクスチャ読み込み
    void createTextures();

    //! メッシュ作成
    void createMesh();

    //! 統計情報を更新
    void computeStatistics();

    //! GPUメッシュをバインド
    void bindGpuMesh(ID3D12GraphicsCommandList* cmd, size_t meshIndex) const;

    //! FlatBuffers 形式でモデルを書き出す
    bool saveFlatBuffer(const std::filesystem::path& filePath) const;

    //! テクスチャを置き換える
    bool replaceTexture(size_t materialIndex, TextureType texType, const std::wstring& newFilePath);

    //! マテリアルのテクスチャをクリアし白テクスチャに戻す
    void clearTexture(size_t materialIndex, TextureType texType);

    //! 内部テクスチャインデックスから LoadTexture を取得（プレビュー用）
    LoadTexture* getTextureByIndex(int texIndex) const
    {
        if (texIndex < 0 || texIndex >= static_cast<int>(m_textures.size())) return nullptr;
        return m_textures[texIndex];
    }

    //! マテリアルの特定スロットに対応する LoadTexture を取得
    LoadTexture* getMaterialTexture(size_t materialIndex, TextureType texType) const;

    //! 読み込んだモデルデータを取得
    const Model& getModelData() const { return m_model; }

    //! 読み込んだモデルデータを取得（編集用）
    Model& getModelData() { return m_model; }

    //! 統計情報を取得
    const Statistics& getStatistics() const { return m_stats; }

private:

    //! サブセットディスクリプタ再構築
    void rebuildSubsetDescriptors(SubMesh& subMesh);

    //! 白テクスチャのインデックスを取得（無ければ生成して登録）
    int getOrCreateWhiteTextureIndex();

    //! GPUメッシュ
    struct GpuMesh
    {
        std::unique_ptr<VertexBuffer<Vertex>> vb;
        std::unique_ptr<IndexBuffer<uint32_t>> ib;
    };
    std::vector<GpuMesh> m_gpuMeshes;

    //! テクスチャ
    std::vector<LoadTexture*> m_textures;
    std::vector<std::wstring> m_texturePaths;

    //! 統計情報
    Statistics  m_stats{};

    static constexpr UINT TEXTURE_SLOT_COUNT = static_cast<UINT>(TextureType::Max);

protected:

    //! 読み込んだモデルデータ
    Model m_model;
};