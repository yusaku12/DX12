#pragma once

#include "Component\Component.h"
#include <PxPhysicsAPI.h>

class RigidbodyComponent;
class TransformComponent;

//=====================================================
// コライダーの形状タイプ
//=====================================================
enum class ColliderShapeType
{
    Box,
    Sphere,
    Capsule,
    Plane,      //!< 無限平面（地面用）
};

//=====================================================
// ColliderComponent
// - PhysX の PxShape をラップ
// - Unity の Collider 基底クラス相当
// - RigidbodyComponent と組み合わせて使用
//=====================================================
class ColliderComponent : public Component
{
public:

    ColliderComponent() = default;
    ~ColliderComponent() override;

    //! 初期化
    void awake() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //========================================
    // 形状設定（awake 後に呼ぶ）
    //========================================

    //! ボックスコライダーとして設定
    void setBoxShape(const Vector3& halfExtents = Vector3(0.5f, 0.5f, 0.5f));

    //! 球コライダーとして設定
    void setSphereShape(float radius = 0.5f);

    //! カプセルコライダーとして設定（Y軸方向）
    void setCapsuleShape(float radius = 0.5f, float halfHeight = 0.5f);

    //! 無限平面コライダーとして設定（地面用）
    void setPlaneShape();

    //========================================
    // プロパティ
    //========================================

    //! トリガーモード（衝突応答なし、イベントのみ）
    void setTrigger(bool isTrigger);
    bool isTrigger() const { return m_isTrigger; }

    //! ローカルオフセット
    void setCenter(const Vector3& center);
    Vector3 getCenter() const { return m_center; }

    //! マテリアル設定
    void setMaterial(float staticFriction, float dynamicFriction, float restitution);

    //! 形状タイプ取得
    ColliderShapeType getShapeType() const { return m_shapeType; }

    //========================================
    // 内部（RigidbodyComponent から参照）
    //========================================

    physx::PxShape* getPxShape() const { return m_shape; }

    //! RigidbodyComponent との紐付け
    void attachToRigidbody(RigidbodyComponent* rb);

private:

    //! 既存シェイプの破棄
    void releaseShape();

    //! シェイプ作成共通処理
    void finalizeShape();

    TransformComponent* m_transform = nullptr;
    RigidbodyComponent* m_rigidbody = nullptr;

    physx::PxShape* m_shape = nullptr;
    physx::PxMaterial* m_material = nullptr;

    ColliderShapeType m_shapeType = ColliderShapeType::Box;
    Vector3 m_center = Vector3::Zero;
    bool m_isTrigger = false;

    //! 形状パラメータ（Inspector 編集用）
    Vector3 m_boxHalfExtents = Vector3(0.5f, 0.5f, 0.5f);
    float m_sphereRadius = 0.5f;
    float m_capsuleRadius = 0.5f;
    float m_capsuleHalfHeight = 0.5f;
};