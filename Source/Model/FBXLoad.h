#pragma once

#include <fbxsdk.h>

//==============================================================
// FBX読み込み・内部データ保持クラス
//==============================================================
class FBXLoad
{
public:

    //! 変換行列分解用構造体
    struct Transform
    {
        Vector3 scale = { 1,1,1 };
        Quaternion rotation = { 0,0,0,1 };
        Vector3 translation = { 0,0,0 };
    };

    //! スケルトン
    struct Skeleton
    {
        struct Bone
        {
            std::string name;          //!< ボーン名（ユニーク）
            int parentIndex = -1;      //!< 親ボーンIndex（-1 = Root）
            Matrix inverseBindMatrix;  //!< Inverse Bind Pose 行列

            bool isRoot() const { return parentIndex < 0; }
        };

        std::vector<Bone> bones;

        //! 高速検索用
        std::unordered_map<std::string, int> boneMap;

        //! ボーン検索（O(1)）
        int findBone(const std::string& name) const
        {
            auto it = boneMap.find(name);
            if (it != boneMap.end())
                return it->second;
            return -1;
        }

        //! スケルトンが空かどうか
        bool empty() const { return bones.empty(); }
    };

    //! キーフレーム
    struct Keyframe
    {
        float time = 0.0f;  //!< 秒単位
        Transform transform;
    };

    //! ボーンアニメーション
    struct BoneAnimation
    {
        std::vector<Keyframe> keyframes;
    };

    //! アニメーションクリップ
    struct AnimationClip
    {
        std::string name;
        float duration = 0.0f;  //!< 秒
        float ticksPerSecond = 30.0f;

        // skeleton.bones と同じ順番で保持
        std::vector<BoneAnimation> boneAnimations;
    };

    //! 頂点
    struct Vertex
    {
        Vector3 position = {};
        Vector3 normal = {};
        Vector4 tangent = {};   //!< w = handedness
        Vector2 uv = {};

        float    boneWeights[4] = { 1,0,0,0 };
        uint32_t boneIndices[4] = { 0,0,0,0 };
    };

    //! サブメッシュ
    struct SubMesh
    {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
        uint32_t materialIndex = 0;
    };

    //! メッシュ
    struct Mesh
    {
        std::string name;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubMesh> subMeshes;

        Matrix bindTransform; //!< ノードのバインド時ワールド行列

        //! AABB計算
        void computeBounds(Vector3& outMin, Vector3& outMax) const
        {
            outMin = Vector3(FLT_MAX);
            outMax = Vector3(-FLT_MAX);
            for (const auto& v : vertices)
            {
                outMin = Vector3::Min(outMin, v.position);
                outMax = Vector3::Max(outMax, v.position);
            }
        }
    };

    //! マテリアル
    struct Material
    {
        std::string name;
        std::string texturePath;
        std::string normalMapPath;

        Vector4 diffuseColor = { 1,1,1,1 };
        Vector3 specularColor = { 0,0,0 };
        float   specularPower = 0.0f;
        Vector3 ambientColor = { 0,0,0 };
        Vector3 emissiveColor = { 0,0,0 };
    };

    //! モデル（最上位コンテナ）
    struct Model
    {
        Skeleton skeleton;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<AnimationClip> animations;

        //! スキンメッシュかどうか
        bool hasSkeleton() const { return !skeleton.empty(); }

        //! アニメーションを持つかどうか
        bool hasAnimation() const { return !animations.empty(); }
    };

    explicit FBXLoad();
    ~FBXLoad();

    //! コピー禁止
    FBXLoad(const FBXLoad&) = delete;
    FBXLoad& operator=(const FBXLoad&) = delete;

    //! ムーブ可
    FBXLoad(FBXLoad&& other) noexcept;
    FBXLoad& operator=(FBXLoad&& other) noexcept;

    //! FBXファイルを読み込む
    bool load(const std::string& path);

    //! 読み込んだモデルデータを取得
    const Model& getModel() const { return m_model; }

    //! 読み込んだモデルデータをムーブで取得
    Model takeModel() { return std::move(m_model); }

private:

    //! FBX SDKの初期化と終了処理
    void initializeFbx();

    //! FBX SDKのリソースを解放
    void destroyFbx();

    //! 座標系補正行列を算出（元の座標系 → DirectX 左手Y-Up）
    FbxAMatrix computeAxisFixMatrix() const;

    //! ノード変換をジオメトリにベイクする（座標系・単位変換の確実な適用）
    void bakeNodeTransforms(FbxNode* node);

    //! FBXファイルを読み込んでシーンを構築
    void parseScene();

    //! FBXのノードからスケルトン構造を構築しつつ、メッシュ情報も抽出
    void parseNode(FbxNode* node, int parentBoneIndex);

    //! FBXのノードからメッシュ情報を抽出してMesh構造に変換
    void parseMesh(FbxNode* node);

    //! FBXのノード階層を再帰的にたどってスケルトン構造を構築
    void parseSkeleton(FbxNode* node, int parentIndex);

    //! FBXのマテリアル情報を抽出してMaterial構造に変換
    void parseMaterials();

    //! FBXのアニメーション情報を抽出してAnimationClip構造に変換
    void parseAnimations();

    //! FBXのスキニング情報を抽出してMesh構造に変換
    void extractSkinning(FbxMesh* mesh, Mesh& dstMesh, const std::unordered_map<int, std::vector<uint32_t>>& ctrlToVertex);

    //! タンジェント自動生成（Tangent がFBXに含まれない場合）
    void generateTangents(Mesh& dstMesh);

    //! コントロールポイントIndex → 展開済み頂点Index のマッピングを構築
    std::unordered_map<int, std::vector<uint32_t>> buildCtrlToVertexMap(FbxMesh* mesh) const;

    Model m_model;
    FbxManager* m_manager = nullptr;
    FbxScene* m_scene = nullptr;
    FbxAMatrix m_axisFixMatrix; //!< 座標系補正行列
};