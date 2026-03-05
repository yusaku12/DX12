#include "pch.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "TransformComponent.h"
#include "GameObject\GameObject.h"
#include "Physics\PhysicsWorld.h"
#include "Physics\PhysXHelper.h"

ColliderComponent::~ColliderComponent()
{
    releaseShape();
}

void ColliderComponent::awake()
{
    m_transform = m_gameObject->getComponent<TransformComponent>();
    if (!m_transform)
    {
        m_transform = m_gameObject->addComponent<TransformComponent>();
    }

    //! デフォルトマテリアルを使用
    m_material = PhysicsWorld::Instance().getDefaultMaterial();

    //! デフォルトでボックスシェイプを作成
    setBoxShape(m_boxHalfExtents);
}

void ColliderComponent::inspectGUI()
{
    //! 形状タイプ選択
    const char* shapeNames[] = { "Box", "Sphere", "Capsule", "Plane" };
    int typeIdx = static_cast<int>(m_shapeType);
    if (ImGui::Combo("Shape", &typeIdx, shapeNames, IM_ARRAYSIZE(shapeNames)))
    {
        ColliderShapeType newType = static_cast<ColliderShapeType>(typeIdx);
        if (newType != m_shapeType)
        {
            switch (newType)
            {
            case ColliderShapeType::Box:     setBoxShape(m_boxHalfExtents); break;
            case ColliderShapeType::Sphere:  setSphereShape(m_sphereRadius); break;
            case ColliderShapeType::Capsule: setCapsuleShape(m_capsuleRadius, m_capsuleHalfHeight); break;
            case ColliderShapeType::Plane:   setPlaneShape(); break;
            }
        }
    }

    //! 形状固有パラメータ
    switch (m_shapeType)
    {
    case ColliderShapeType::Box:
        if (ImGui::DragFloat3("Half Extents", &m_boxHalfExtents.x, 0.01f, 0.01f, 100.0f))
            setBoxShape(m_boxHalfExtents);
        break;

    case ColliderShapeType::Sphere:
        if (ImGui::DragFloat("Radius", &m_sphereRadius, 0.01f, 0.01f, 100.0f))
            setSphereShape(m_sphereRadius);
        break;

    case ColliderShapeType::Capsule:
        bool changed = false;
        changed |= ImGui::DragFloat("Capsule Radius", &m_capsuleRadius, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat("Capsule HalfHeight", &m_capsuleHalfHeight, 0.01f, 0.01f, 100.0f);
        if (changed) setCapsuleShape(m_capsuleRadius, m_capsuleHalfHeight);
        break;
    }

    //! 共通設定
    if (ImGui::DragFloat3("Center", &m_center.x, 0.01f))
        setCenter(m_center);

    if (ImGui::Checkbox("Is Trigger", &m_isTrigger))
        setTrigger(m_isTrigger);
}

//=====================================================
// 形状設定
//=====================================================
void ColliderComponent::setBoxShape(const Vector3& halfExtents)
{
    releaseShape();
    m_shapeType = ColliderShapeType::Box;
    m_boxHalfExtents = halfExtents;

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxBoxGeometry geom(PhysXHelper::toPx(halfExtents));
    m_shape = physics->createShape(geom, *m_material, true);
    DebugPrimitive::Instance().addBox(Vector3::Zero, halfExtents, { 0,1,0,1 }, 0.0f); //!< デバッグ描画用
    finalizeShape();
}

void ColliderComponent::setSphereShape(float radius)
{
    releaseShape();
    m_shapeType = ColliderShapeType::Sphere;
    m_sphereRadius = radius;

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxSphereGeometry geom(radius);
    m_shape = physics->createShape(geom, *m_material, true);
    //DebugPrimitive::Instance().addSphere() //!< デバッグ描画用
    finalizeShape();
}

void ColliderComponent::setCapsuleShape(float radius, float halfHeight)
{
    releaseShape();
    m_shapeType = ColliderShapeType::Capsule;
    m_capsuleRadius = radius;
    m_capsuleHalfHeight = halfHeight;

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxCapsuleGeometry geom(radius, halfHeight);
    m_shape = physics->createShape(geom, *m_material, true);

    //! PhysX のカプセルは X 軸方向なので Y 軸方向に回転
    physx::PxTransform localPose(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f)));
    m_shape->setLocalPose(localPose);
    finalizeShape();
}

void ColliderComponent::setPlaneShape()
{
    releaseShape();
    m_shapeType = ColliderShapeType::Plane;

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxPlaneGeometry geom;
    m_shape = physics->createShape(geom, *m_material, true);
    finalizeShape();
}

//=====================================================
// プロパティ設定
//=====================================================
void ColliderComponent::setTrigger(bool isTrigger)
{
    m_isTrigger = isTrigger;
    if (!m_shape) return;

    if (isTrigger)
    {
        m_shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
    }
    else
    {
        m_shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        m_shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
    }
}

void ColliderComponent::setCenter(const Vector3& center)
{
    m_center = center;
    if (!m_shape) return;

    physx::PxTransform localPose = m_shape->getLocalPose();
    localPose.p = PhysXHelper::toPx(center);
    m_shape->setLocalPose(localPose);
}

void ColliderComponent::setMaterial(float staticFriction, float dynamicFriction, float restitution)
{
    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics) return;

    m_material = physics->createMaterial(staticFriction, dynamicFriction, restitution);
    if (m_shape)
    {
        m_shape->setMaterials(&m_material, 1);
    }
}

//=====================================================
// RigidbodyComponent との紐付け
//=====================================================
void ColliderComponent::attachToRigidbody(RigidbodyComponent* rb)
{
    m_rigidbody = rb;
}

//=====================================================
// 内部
//=====================================================
void ColliderComponent::releaseShape()
{
    if (m_shape)
    {
        m_shape->release();
        m_shape = nullptr;
    }
}

void ColliderComponent::finalizeShape()
{
    if (!m_shape) return;

    //! デバッグ描画用フラグ
    m_shape->setFlag(physx::PxShapeFlag::eVISUALIZATION, true);

    //! トリガー設定の反映
    setTrigger(m_isTrigger);

    //! センターオフセットの反映
    if (m_center.LengthSquared() > 1e-6f)
    {
        setCenter(m_center);
    }
}