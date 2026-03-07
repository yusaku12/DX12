#pragma once

#include "Component\Component.h"
#include <PxPhysicsAPI.h>

class TransformComponent;
class ColliderComponent;

//=====================================================
// 剛体の種類（Unity の RigidbodyType 相当）
//=====================================================
enum class RigidbodyType
{
    Dynamic,    //!< 物理演算で動く
    Kinematic,  //!< スクリプトで動かす（他の物体を押せる）
    Static,     //!< 動かない
};

//=====================================================
// RigidbodyComponent
// - PhysX の PxRigidActor をラップ
// - TransformComponent / ColliderComponent と自動連携
// - Unity の Rigidbody 相当
//=====================================================
class RigidbodyComponent : public Component
{
public:

    RigidbodyComponent() = default;
    ~RigidbodyComponent() override;

    //! 初期化
    void awake() override;

    //! PhysX アクター作成（全コンポーネントの awake 完了後に呼ばれる）
    void start() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! 破棄時
    void onDestroy() override;

    //! 力を加える（連続力 = Force モード）
    void addForce(const Vector3& force, physx::PxForceMode::Enum mode = physx::PxForceMode::eFORCE);

    //! トルクを加える
    void addTorque(const Vector3& torque, physx::PxForceMode::Enum mode = physx::PxForceMode::eFORCE);

    //! 線形速度の設定・取得
    void setLinearVelocity(const Vector3& vel);
    Vector3 getLinearVelocity() const;

    //! 角速度の設定・取得
    void setAngularVelocity(const Vector3& vel);
    Vector3 getAngularVelocity() const;

    //! 質量の設定・取得
    void setMass(float mass);
    float getMass() const { return m_mass; }

    //! 線形減衰の設定・取得
    void setLinearDrag(float drag);
    float getLinearDrag() const { return m_linearDrag; }

    //! 角減衰の設定・取得
    void setAngularDrag(float drag);
    float getAngularDrag() const { return m_angularDrag; }

    //! 重力の使用設定・取得
    void setUseGravity(bool use);
    bool getUseGravity() const { return m_useGravity; }

    //! Kinematic 設定・取得
    void setKinematic(bool kinematic);
    bool isKinematic() const { return m_isKinematic; }

    //! 剛体の種類設定・取得
    void setType(RigidbodyType type);
    RigidbodyType getType() const { return m_type; }

    //! 回転軸のロック (true = ロック)
    void setFreezeRotationX(bool freeze);
    void setFreezeRotationY(bool freeze);
    void setFreezeRotationZ(bool freeze);

    //! 移動軸のロック (true = ロック)
    void setFreezePositionX(bool freeze);
    void setFreezePositionY(bool freeze);
    void setFreezePositionZ(bool freeze);

    //! TransformComponent から位置・回転の移動
    void movePosition(const Vector3& pos);
    void moveRotation(const Quaternion& rot);

    //! PhysX -> TransformComponent への同期
    void syncFromPhysics();

    //! Dynamic アクターのスリープを解除する
    void wakeUp();

    //! TransformComponent -> PhysX への同期（Kinematic / Static 用）
    void syncToPhysics();

    //! PhysX アクター取得
    physx::PxRigidActor* getPxActor() const { return m_actor; }

    //! Transform 取得
    TransformComponent* getTransform() const { return m_transform; }

private:

    //! PhysX アクターの作成
    void createActor();

    //! PhysX アクターの破棄
    void releaseActor();

    //! ロックフラグの更新
    void updateLockFlags();

    TransformComponent* m_transform = nullptr;

    physx::PxRigidActor* m_actor = nullptr;

    RigidbodyType m_type = RigidbodyType::Dynamic;
    bool m_isKinematic = false;
    float m_mass = 1.0f;
    float m_linearDrag = 0.0f;
    float m_angularDrag = 0.05f;
    bool m_useGravity = true;

    //! 軸ロック
    bool m_freezePosX = false, m_freezePosY = false, m_freezePosZ = false;
    bool m_freezeRotX = false, m_freezeRotY = false, m_freezeRotZ = false;
};