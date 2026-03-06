#include "pch.h"
#include "RigidbodyComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "GameObject\GameObject.h"
#include "Physics\PhysicsWorld.h"
#include "Physics\PhysXHelper.h"

RigidbodyComponent::~RigidbodyComponent()
{
    releaseActor();
}

void RigidbodyComponent::awake()
{
    //! TransformComponent を自動取得（なければ追加）
    m_transform = m_gameObject->getComponent<TransformComponent>();
    if (!m_transform)
    {
        m_transform = m_gameObject->addComponent<TransformComponent>();
    }

    createActor();
}

void RigidbodyComponent::onDestroy()
{
    //! ColliderComponent との紐付けを解除
    auto* collider = m_gameObject->getComponent<ColliderComponent>();
    if (collider)
    {
        collider->detachFromRigidbody();
    }

    releaseActor();
}

void RigidbodyComponent::createActor()
{
    auto& world = PhysicsWorld::Instance();
    auto* physics = world.getPhysics();
    auto* scene = world.getScene();
    if (!physics || !scene) return;

    //! 現在の TransformComponent から初期姿勢を取得
    physx::PxTransform pose = PhysXHelper::ToPxTransform(
        m_transform->getPosition(),
        m_transform->getRotation()
    );

    //! タイプに応じてアクターを作成
    bool useKinematic = m_isKinematic || m_type == RigidbodyType::Kinematic;

    if (m_type == RigidbodyType::Static)
    {
        auto* staticActor = physics->createRigidStatic(pose);
        m_actor = staticActor;
    }
    else
    {
        auto* dynamicActor = physics->createRigidDynamic(pose);

        dynamicActor->setMass(m_mass);
        dynamicActor->setLinearDamping(m_linearDrag);
        dynamicActor->setAngularDamping(m_angularDrag);
        dynamicActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !m_useGravity);

        if (useKinematic)
        {
            dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
        }

        m_actor = dynamicActor;
        updateLockFlags();
    }

    //! userData にこのコンポーネントへのポインタを設定（衝突コールバック用）
    m_actor->userData = this;

    //! ColliderComponent があればシェイプをアタッチし、相互紐付けする
    auto* collider = m_gameObject->getComponent<ColliderComponent>();
    bool hasValidCollider = collider && collider->getPxShape();

    if (hasValidCollider)
    {
        m_actor->attachShape(*collider->getPxShape());
        collider->attachToRigidbody(this);
    }
    else
    {
        //! ColliderComponent がない、またはシェイプ未設定の場合はデフォルトボックス
        physx::PxBoxGeometry defaultGeom(0.5f, 0.5f, 0.5f);
        auto* defaultShape = physics->createShape(defaultGeom, *world.getDefaultMaterial(), true);
        m_actor->attachShape(*defaultShape);
        defaultShape->release();
    }

    //! Dynamic の場合は質量特性を自動計算
    if (m_actor->getType() == physx::PxActorType::eRIGID_DYNAMIC && !useKinematic)
    {
        bool canComputeMass = true;
        if (hasValidCollider)
        {
            canComputeMass = collider->getShapeType() != ColliderShapeType::Plane
                && !collider->isTrigger();
        }

        if (canComputeMass)
        {
            auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);
            physx::PxRigidBodyExt::updateMassAndInertia(*dyn, m_mass);
        }
    }

    //! シーンに追加
    scene->addActor(*m_actor);
}

void RigidbodyComponent::releaseActor()
{
    if (m_actor)
    {
        auto* scene = PhysicsWorld::Instance().getScene();
        if (scene)
        {
            scene->removeActor(*m_actor);
        }
        m_actor->release();
        m_actor = nullptr;
    }
}

void RigidbodyComponent::inspectGUI()
{
    const char* typeNames[] = { "Dynamic", "Kinematic", "Static" };
    int typeIdx = static_cast<int>(m_type);
    if (ImGui::Combo("Body Type", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames)))
    {
        setType(static_cast<RigidbodyType>(typeIdx));
    }

    if (m_type != RigidbodyType::Static)
    {
        if (ImGui::DragFloat("Mass", &m_mass, 0.1f, 0.001f, 10000.0f))
            setMass(m_mass);

        if (ImGui::DragFloat("Linear Drag", &m_linearDrag, 0.01f, 0.0f, 100.0f))
            setLinearDrag(m_linearDrag);

        if (ImGui::DragFloat("Angular Drag", &m_angularDrag, 0.01f, 0.0f, 100.0f))
            setAngularDrag(m_angularDrag);

        if (ImGui::Checkbox("Use Gravity", &m_useGravity))
            setUseGravity(m_useGravity);

        if (m_type == RigidbodyType::Dynamic)
        {
            if (ImGui::Checkbox("Is Kinematic", &m_isKinematic))
                setKinematic(m_isKinematic);
        }

        ImGui::Separator();
        ImGui::Text("Freeze Position:");
        ImGui::SameLine();
        if (ImGui::Checkbox("X##pos", &m_freezePosX)) setFreezePositionX(m_freezePosX);
        ImGui::SameLine();
        if (ImGui::Checkbox("Y##pos", &m_freezePosY)) setFreezePositionY(m_freezePosY);
        ImGui::SameLine();
        if (ImGui::Checkbox("Z##pos", &m_freezePosZ)) setFreezePositionZ(m_freezePosZ);

        ImGui::Text("Freeze Rotation:");
        ImGui::SameLine();
        if (ImGui::Checkbox("X##rot", &m_freezeRotX)) setFreezeRotationX(m_freezeRotX);
        ImGui::SameLine();
        if (ImGui::Checkbox("Y##rot", &m_freezeRotY)) setFreezeRotationY(m_freezeRotY);
        ImGui::SameLine();
        if (ImGui::Checkbox("Z##rot", &m_freezeRotZ)) setFreezeRotationZ(m_freezeRotZ);
    }

    ImGui::Separator();
    Vector3 linVel = getLinearVelocity();
    Vector3 angVel = getAngularVelocity();
    ImGui::Text("Linear Vel : (%.2f, %.2f, %.2f)", linVel.x, linVel.y, linVel.z);
    ImGui::Text("Angular Vel: (%.2f, %.2f, %.2f)", angVel.x, angVel.y, angVel.z);
}

void RigidbodyComponent::addForce(const Vector3& force, physx::PxForceMode::Enum mode)
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);
    if (dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC) return;
    dyn->addForce(PhysXHelper::ToPxVec3(force), mode);
}

void RigidbodyComponent::addTorque(const Vector3& torque, physx::PxForceMode::Enum mode)
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);
    if (dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC) return;
    dyn->addTorque(PhysXHelper::ToPxVec3(torque), mode);
}

void RigidbodyComponent::setLinearVelocity(const Vector3& vel)
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    static_cast<physx::PxRigidDynamic*>(m_actor)->setLinearVelocity(PhysXHelper::ToPxVec3(vel));
}

Vector3 RigidbodyComponent::getLinearVelocity() const
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return Vector3::Zero;
    return PhysXHelper::ToVector3(static_cast<physx::PxRigidDynamic*>(m_actor)->getLinearVelocity());
}

void RigidbodyComponent::setAngularVelocity(const Vector3& vel)
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    static_cast<physx::PxRigidDynamic*>(m_actor)->setAngularVelocity(PhysXHelper::ToPxVec3(vel));
}

Vector3 RigidbodyComponent::getAngularVelocity() const
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return Vector3::Zero;
    return PhysXHelper::ToVector3(static_cast<physx::PxRigidDynamic*>(m_actor)->getAngularVelocity());
}

void RigidbodyComponent::setMass(float mass)
{
    m_mass = mass;
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);
    dyn->setMass(mass);

    auto* collider = m_gameObject->getComponent<ColliderComponent>();
    if (collider && (collider->getShapeType() == ColliderShapeType::Plane || collider->isTrigger()))
        return;

    physx::PxRigidBodyExt::updateMassAndInertia(*dyn, mass);
}

void RigidbodyComponent::setLinearDrag(float drag)
{
    m_linearDrag = drag;
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    static_cast<physx::PxRigidDynamic*>(m_actor)->setLinearDamping(drag);
}

void RigidbodyComponent::setAngularDrag(float drag)
{
    m_angularDrag = drag;
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    static_cast<physx::PxRigidDynamic*>(m_actor)->setAngularDamping(drag);
}

void RigidbodyComponent::setUseGravity(bool use)
{
    m_useGravity = use;
    if (!m_actor) return;
    m_actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !use);
}

void RigidbodyComponent::setKinematic(bool kinematic)
{
    m_isKinematic = kinematic;
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    static_cast<physx::PxRigidDynamic*>(m_actor)->setRigidBodyFlag(
        physx::PxRigidBodyFlag::eKINEMATIC, kinematic);
}

void RigidbodyComponent::setType(RigidbodyType type)
{
    if (m_type == type) return;
    m_type = type;
    if (type == RigidbodyType::Kinematic) m_isKinematic = true;
    releaseActor();
    createActor();
}

void RigidbodyComponent::setFreezePositionX(bool freeze) { m_freezePosX = freeze; updateLockFlags(); }
void RigidbodyComponent::setFreezePositionY(bool freeze) { m_freezePosY = freeze; updateLockFlags(); }
void RigidbodyComponent::setFreezePositionZ(bool freeze) { m_freezePosZ = freeze; updateLockFlags(); }
void RigidbodyComponent::setFreezeRotationX(bool freeze) { m_freezeRotX = freeze; updateLockFlags(); }
void RigidbodyComponent::setFreezeRotationY(bool freeze) { m_freezeRotY = freeze; updateLockFlags(); }
void RigidbodyComponent::setFreezeRotationZ(bool freeze) { m_freezeRotZ = freeze; updateLockFlags(); }

void RigidbodyComponent::updateLockFlags()
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);

    physx::PxRigidDynamicLockFlags flags;
    if (m_freezePosX) flags |= physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X;
    if (m_freezePosY) flags |= physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y;
    if (m_freezePosZ) flags |= physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
    if (m_freezeRotX) flags |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
    if (m_freezeRotY) flags |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
    if (m_freezeRotZ) flags |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;

    dyn->setRigidDynamicLockFlags(flags);
}

void RigidbodyComponent::movePosition(const Vector3& pos)
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);
    physx::PxTransform t = dyn->getGlobalPose();
    t.p = PhysXHelper::ToPxVec3(pos);
    dyn->setKinematicTarget(t);
}

void RigidbodyComponent::moveRotation(const Quaternion& rot)
{
    if (!m_actor || m_actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) return;
    auto* dyn = static_cast<physx::PxRigidDynamic*>(m_actor);
    physx::PxTransform t = dyn->getGlobalPose();
    t.q = PhysXHelper::ToPxQuat(rot);
    dyn->setKinematicTarget(t);
}

void RigidbodyComponent::syncFromPhysics()
{
    if (!m_actor || !m_transform) return;

    physx::PxTransform pose = m_actor->getGlobalPose();
    m_transform->setPosition(PhysXHelper::ToVector3(pose.p));
    m_transform->setRotation(PhysXHelper::ToQuaternion(pose.q));
}

void RigidbodyComponent::syncToPhysics()
{
    if (!m_actor || !m_transform) return;

    physx::PxTransform pose = PhysXHelper::ToPxTransform(
        m_transform->getPosition(),
        m_transform->getRotation()
    );
    m_actor->setGlobalPose(pose);
}