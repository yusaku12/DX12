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

    //! 初期化（TransformComponent の取得とマテリアル設定のみ）
    void awake() override;

    //! 毎フレーム後処理（デバッグ描画の自動更新）
    void lateUpdate() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! ボックスコライダーとして設定
    void setBoxShape(const Vector3& halfExtents = Vector3(0.5f, 0.5f, 0.5f));

    //! 球コライダーとして設定
    void setSphereShape(float radius = 0.5f);

    //! カプセルコライダーとして設定（Y軸方向）
    void setCapsuleShape(float radius = 0.5f, float halfHeight = 0.5f);

    //! 無限平面コライダーとして設定（地面用）
    void setPlaneShape();

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

    //! RigidbodyComponent との紐付け
    void attachToRigidbody(RigidbodyComponent* rb);

    //! RigidbodyComponent の紐付け解除
    void detachFromRigidbody();

    //! PhysX の PxShape 取得
    physx::PxShape* getPxShape() const { return m_shape; }

    //! デバッグ描画の有効・無効
    void setDebugDraw(bool enable) { m_debugDraw = enable; }
    bool isDebugDraw() const { return m_debugDraw; }

    //! 形状パラメータ取得（シリアライズ用）
    const Vector3& getBoxHalfExtents() const { return m_boxHalfExtents; }
    float getSphereRadius() const { return m_sphereRadius; }
    float getCapsuleRadius() const { return m_capsuleRadius; }
    float getCapsuleHalfHeight() const { return m_capsuleHalfHeight; }

    //! このコライダーのワールド空間での AABB を取得
    physx::PxBounds3 getBounds() const;

private:

    //! ジオメトリを指定してシェイプを作成
    template<typename Geometry>
    void createShape(const Geometry& geom);

    //! 既存シェイプの破棄（アクターからデタッチも行う）
    void releaseShape();

    //! シェイプ作成共通処理
    void finalizeShape();

    //! シェイプをアクターに再アタッチ
    void reattachShapeToActor();

    //! デバッグ描画（毎フレーム自動で呼ばれる）
    void drawDebugShape();

    TransformComponent* m_transform = nullptr;
    RigidbodyComponent* m_rigidbody = nullptr;

    physx::PxShape* m_shape = nullptr;
    physx::PxMaterial* m_material = nullptr;

    ColliderShapeType m_shapeType = ColliderShapeType::Box;
    Vector3 m_center = Vector3::Zero;
    bool m_isTrigger = false;
    bool m_debugDraw = true;

    //! 形状パラメータ（Inspector 編集用）
    Vector3 m_boxHalfExtents = Vector3(0.5f, 0.5f, 0.5f);
    float m_sphereRadius = 0.5f;
    float m_capsuleRadius = 0.5f;
    float m_capsuleHalfHeight = 0.5f;
};