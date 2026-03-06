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

    //! awake 時点ではシェイプを作成しない
    //! 利用側が setBoxShape / setSphereShape 等を呼ぶことでシェイプが作られる
    //! RigidbodyComponent::awake() が ColliderComponent のシェイプを拾う順序を保証するため
}

void ColliderComponent::lateUpdate()
{
    //! 毎フレーム自動で現在位置にデバッグ描画
    drawDebugShape();
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
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Capsule Radius", &m_capsuleRadius, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat("Capsule HalfHeight", &m_capsuleHalfHeight, 0.01f, 0.01f, 100.0f);
        if (changed) setCapsuleShape(m_capsuleRadius, m_capsuleHalfHeight);
        break;
    }

    default:
        break;
    }

    //! 共通設定
    if (ImGui::DragFloat3("Center", &m_center.x, 0.01f))
        setCenter(m_center);

    if (ImGui::Checkbox("Is Trigger", &m_isTrigger))
        setTrigger(m_isTrigger);

    ImGui::Checkbox("Debug Draw", &m_debugDraw);
}

void ColliderComponent::setBoxShape(const Vector3& halfExtents)
{
    releaseShape();
    m_shapeType = ColliderShapeType::Box;

    m_boxHalfExtents = Vector3(
        std::max(halfExtents.x, 0.001f),
        std::max(halfExtents.y, 0.001f),
        std::max(halfExtents.z, 0.001f)
    );

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxBoxGeometry geom(PhysXHelper::ToPxVec3(m_boxHalfExtents));
    m_shape = physics->createShape(geom, *m_material, true);
    if (!m_shape) return;

    finalizeShape();
}

void ColliderComponent::setSphereShape(float radius)
{
    releaseShape();
    m_shapeType = ColliderShapeType::Sphere;
    m_sphereRadius = std::max(radius, 0.001f);

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxSphereGeometry geom(m_sphereRadius);
    m_shape = physics->createShape(geom, *m_material, true);
    if (!m_shape) return;

    finalizeShape();
}

void ColliderComponent::setCapsuleShape(float radius, float halfHeight)
{
    releaseShape();
    m_shapeType = ColliderShapeType::Capsule;
    m_capsuleRadius = std::max(radius, 0.001f);
    m_capsuleHalfHeight = std::max(halfHeight, 0.001f);

    auto* physics = PhysicsWorld::Instance().getPhysics();
    if (!physics || !m_material) return;

    physx::PxCapsuleGeometry geom(m_capsuleRadius, m_capsuleHalfHeight);
    m_shape = physics->createShape(geom, *m_material, true);
    if (!m_shape) return;

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
    if (!m_shape) return;

    finalizeShape();
}

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
    localPose.p = PhysXHelper::ToPxVec3(center);
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

void ColliderComponent::attachToRigidbody(RigidbodyComponent* rb)
{
    m_rigidbody = rb;
}

void ColliderComponent::detachFromRigidbody()
{
    m_rigidbody = nullptr;
}

void ColliderComponent::releaseShape()
{
    if (m_shape)
    {
        physx::PxRigidActor* actor = m_shape->getActor();
        if (actor)
        {
            actor->detachShape(*m_shape);
        }
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

    //! 既にアクターが存在すればシェイプを再アタッチ
    reattachShapeToActor();
}

void ColliderComponent::reattachShapeToActor()
{
    if (!m_shape || !m_rigidbody) return;

    auto* actor = m_rigidbody->getPxActor();
    if (!actor) return;

    actor->attachShape(*m_shape);

    if (actor->getType() == physx::PxActorType::eRIGID_DYNAMIC)
    {
        auto* dyn = static_cast<physx::PxRigidDynamic*>(actor);
        if (!(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)
            && m_shapeType != ColliderShapeType::Plane
            && !m_isTrigger)
        {
            physx::PxRigidBodyExt::updateMassAndInertia(*dyn, m_rigidbody->getMass());
        }
    }
}

void ColliderComponent::drawDebugShape()
{
    if (!m_debugDraw || !m_shape || !m_transform) return;

    //! ワールド行列からスケールを除去（回転 + 平行移動のみ）
    //! コライダーのサイズは halfExtents / radius / halfHeight で表現されるため
    //! Transform のスケールを含めると二重適用になる
    Matrix worldMat = m_transform->getWorldMatrix();

    Vector3 scale;
    Quaternion rot;
    Vector3 pos;
    worldMat.Decompose(scale, rot, pos);

    //! センターオフセットを回転後のローカル空間で適用
    Vector3 rotatedCenter = Vector3::Transform(m_center, rot);
    Matrix world = Matrix::CreateFromQuaternion(rot) * Matrix::CreateTranslation(pos + rotatedCenter);

    const Vector4 color = m_isTrigger ? Vector4(1, 1, 0, 1) : Vector4(0, 1, 0, 1);

    auto& dbg = DebugPrimitive::Instance();

    switch (m_shapeType)
    {
    case ColliderShapeType::Box:
        dbg.drawBox(world, m_boxHalfExtents, color);
        break;

    case ColliderShapeType::Sphere:
        dbg.drawSphere(world, m_sphereRadius, color);
        break;

    case ColliderShapeType::Capsule:
        dbg.drawCapsule(world, m_capsuleRadius, m_capsuleHalfHeight, color);
        break;

    case ColliderShapeType::Plane:
        break;
    }
}