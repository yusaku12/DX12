#pragma once

#include "ModelResource.h"

//------------------------------------------------------------------------------
// モデル更新を行うクラス
//------------------------------------------------------------------------------
class Model
{
public:

    explicit Model(std::shared_ptr<ModelResource> resource);
    ~Model() {}

    //! 実際に使うボーン構造体
    struct Bone
    {
        std::string name;
        Bone* parent;
        Vector3	scale = {};
        Vector4	rotate = {};
        Vector3	translate = {};
        Matrix	localTransform = {};
        Matrix	worldTransform = {};

        std::vector<Bone*>	children;
    };

    //! 行列計算
    void updateTransform(const Matrix& transform);

    //! モデルリソースへのアクセス
    const std::shared_ptr<ModelResource>& getResource() const { return m_resource; }

    //! モデル全体のローカル空間 AABB を取得
    bool getLocalAABB(Vector3& outCenter, Vector3& outExtents) const;

    //! ボーン構造体へのアクセス
    const std::vector<Bone>& getBone() const { return m_bones; }
    std::vector<Bone>& getMutableBone() { return m_bones; }

private:

    //! 初期姿勢の全メッシュからモデル全体の AABB を構築
    void rebuildLocalAABB();

    std::shared_ptr<ModelResource> m_resource;
    std::vector<Bone>				m_bones;
    Vector3 m_localAABBCenter = Vector3::Zero;
    Vector3 m_localAABBExtents = Vector3::Zero;
    bool m_hasLocalAABB = false;
};