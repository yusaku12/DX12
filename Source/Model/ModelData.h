#pragma once
#include "PmxLoad.h"
#include "FbxLoad.h"

//! .mdl ファイルマジック
static constexpr uint32_t MDL_MAGIC = 'M' | ('D' << 8) | ('L' << 16) | ('1' << 24);
static constexpr uint32_t MDL_VERSION = 1;

//=====================================================
// GPU 送信用の頂点構造体（32bit アラインメント）
//=====================================================
struct ModelVertex
{
    Vector3  position;
    Vector3  normal;
    Vector4  tangent;     //!< w = handedness
    Vector2  uv;
    uint32_t boneIndices[4] = { 0, 0, 0, 0 };
    float    boneWeights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
};

//=====================================================
// サブメッシュ
//=====================================================
struct ModelSubMesh
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

//=====================================================
// メッシュ
//=====================================================
struct ModelMesh
{
    std::string              name;
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t>    indices;
    std::vector<ModelSubMesh> subMeshes;
    Vector3                  boundsMin = {};
    Vector3                  boundsMax = {};
};

//=====================================================
// マテリアル
//=====================================================
struct ModelMaterial
{
    std::string name;
    Vector4     diffuse = { 1, 1, 1, 1 };
    Vector3     specular = { 0, 0, 0 };
    float       specularPower = 0.0f;
    Vector3     ambient = { 0, 0, 0 };
    Vector3     emissive = { 0, 0, 0 };
    std::string texturePath[2] = {};
    bool isVisible = true;
};

//=====================================================
// 統一モデルデータ
//=====================================================
struct ModelData
{
    std::string                name;
    std::vector<ModelMesh>     meshes;
    std::vector<ModelMaterial> materials;

    //! AABB 全体
    Vector3 boundsMin = {};
    Vector3 boundsMax = {};

    //! AABB 再計算
    void computeBounds();

    //! PMX からインポート
    static ModelData importFromPMX(const PmxLoad::PMXFileData& pmx);

    //! FBX からインポート
    static ModelData importFromFBX(const FbxLoad::Model& fbx);

    //! .mdl バイナリ保存
    bool saveToMdl(const std::string& path) const;

    //! .mdl バイナリ読み込み
    static bool loadFromMdl(const std::string& path, ModelData& outData);
};
