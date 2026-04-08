#pragma once

#include <fbxsdk.h>
#include "ModelResource.h"

//==============================================================
// FBX読み込み・内部データ保持クラス
//==============================================================
class FbxLoad :public ModelResource
{
public:

    explicit FbxLoad() {};
    ~FbxLoad() override {};

    //! FBXファイルを読み込む
    bool load(const char* filename);

    //! アニメーション追加読み込み
    void addAnimation(const char* filename);

private:

    //! ノードデータを構築
    void loadNodes(FbxNode* fbxNode, int parentNodeIndex);
    void loadNode(FbxNode* fbxNode, int parentNodeIndex);

    //! メッシュデータを読み込み
    void loadMeshes(FbxNode* fbxNode);
    void loadMesh(FbxNode* fbxNode, FbxMesh* fbxMesh);

    //! マテリアルデータを読み込み
    void loadMaterials(const char* dirname, FbxScene* fbxScene);
    void loadMaterial(const char* dirname, FbxSurfaceMaterial* fbxSurfaceMaterial);

    //! アニメーションデータを読み込み
    void loadAnimations(FbxScene* fbxScene, const char* name, bool append);

    //! 移動値を右手座標系から左手座標系へ変換する
    void convertTranslationFromRHtoLH(Vector3& translate);

    //! 回転値を右手座標系から左手座標系へ変換する
    void convertRotationFromRHtoLH(Vector4& rotate);

    //! 行列値を右手座標系から左手座標系へ変換する
    void convertMatrixFromRHtoLH(Matrix& matrix);

    //! インデックスバッファを右手座標系から左手座標系へ変換する
    void convertIndexBufferFromRHtoLH(std::vector<UINT>& indices);

    //! ノードパス取得
    std::string getNodePath(FbxNode* fbxNode) const;

    //! ノードID取得
    UINT64 getNodeId(FbxNode* fbxNode);

    //! ノードインデックスを取得する
    int findNodeIndex(UINT64 nodeId) const;

    //! マテリアルインデックスを取得する
    int findMaterialIndex(FbxScene* fbxScene, const FbxSurfaceMaterial* fbxSurfaceMaterial);

    //! ルートモーションノードのインデックス（-1の場合はルートモーションなし）
    int rootMotionNodeIndex = -1;
};