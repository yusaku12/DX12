#include "pch.h"
#include "CharacterControllerComponent.h"
#include "TransformComponent.h"
#include "GameObject\GameObject.h"
#include "Physics\PhysicsWorld.h"
#include "Physics\PhysXHelper.h"

CharacterControllerComponent::~CharacterControllerComponent()
{
    releaseController();
}

void CharacterControllerComponent::awake()
{
    //! TransformComponent を自動取得（なければ追加）
    m_transform = m_gameObject->getComponent<TransformComponent>();
    if (!m_transform)
    {
        m_transform = m_gameObject->addComponent<TransformComponent>();
    }
}

void CharacterControllerComponent::start()
{
    if (!m_controller)
    {
        createController();
    }
}

void CharacterControllerComponent::update()
{
    if (!m_controller) return;

    float dt = TimeManager::Instance().getDeltaTime();

    //! 重力を適用
    if (m_useGravity)
    {
        if (m_collisionFlags.below)
        {
            //! 接地中は垂直速度をリセット（わずかな下向き力で接地を維持）
            m_verticalVelocity = -0.1f;
        }
        else
        {
            //! 空中では重力を加速
            Vector3 gravity = PhysicsWorld::Instance().getGravity();
            m_verticalVelocity += gravity.y * m_gravityScale * dt;
        }

        //! 重力による移動を適用
        Vector3 gravityDisplacement(0.0f, m_verticalVelocity * dt, 0.0f);
        move(gravityDisplacement);
    }
}

void CharacterControllerComponent::lateUpdate()
{
    if (!m_controller || !m_transform) return;

    //! PhysX コントローラーの位置を TransformComponent に反映
    Vector3 currentPos = getPosition();
    m_transform->setPosition(currentPos);

    //! 速度計算（前フレームとの位置差分）
    float dt = TimeManager::Instance().getDeltaTime();
    if (dt > 0.0f)
    {
        m_velocity = (currentPos - m_previousPosition) / dt;
    }
    m_previousPosition = currentPos;
}

void CharacterControllerComponent::inspectGUI()
{
    ImGui::Text("=== Character Controller ===");

    //! カプセル形状
    float radius = m_radius;
    if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f))
        setRadius(radius);

    float height = m_height;
    if (ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 10.0f))
        setHeight(height);

    //! 段差・坂道
    float stepOff = m_stepOffset;
    if (ImGui::DragFloat("Step Offset", &stepOff, 0.01f, 0.0f, 5.0f))
        setStepOffset(stepOff);

    float slopeLimit = m_slopeLimitDeg;
    if (ImGui::DragFloat("Slope Limit (deg)", &slopeLimit, 0.5f, 0.0f, 90.0f))
        setSlopeLimit(slopeLimit);

    float contactOff = m_contactOffset;
    if (ImGui::DragFloat("Contact Offset", &contactOff, 0.001f, 0.001f, 1.0f))
        setContactOffset(contactOff);

    //! 重力
    ImGui::Checkbox("Use Gravity", &m_useGravity);
    ImGui::DragFloat("Gravity Scale", &m_gravityScale, 0.1f, 0.0f, 10.0f);

    //! 坂道スライド
    ImGui::Checkbox("Slide On Slopes", &m_slideOnSlopes);
    if (m_slideOnSlopes)
    {
        ImGui::DragFloat("Slide Factor", &m_slideFactor, 0.1f, 0.0f, 10.0f);
    }

    ImGui::DragFloat("Min Move Distance", &m_minMoveDistance, 0.0001f, 0.0f, 1.0f, "%.4f");

    //! ステータス表示
    ImGui::Separator();
    ImGui::Text("Grounded     : %s", m_collisionFlags.below ? "Yes" : "No");
    ImGui::Text("Ceiling Hit  : %s", m_collisionFlags.above ? "Yes" : "No");
    ImGui::Text("Wall Hit     : %s", m_collisionFlags.sides ? "Yes" : "No");
    ImGui::Text("Vertical Vel : %.2f", m_verticalVelocity);
    ImGui::Text("Velocity     : (%.2f, %.2f, %.2f)", m_velocity.x, m_velocity.y, m_velocity.z);
    ImGui::Text("Ground Normal: (%.2f, %.2f, %.2f)", m_groundNormal.x, m_groundNormal.y, m_groundNormal.z);
}

void CharacterControllerComponent::onDestroy()
{
    releaseController();
}

void CharacterControllerComponent::createController()
{
    auto& world = PhysicsWorld::Instance();
    auto* manager = world.getControllerManager();
    auto* material = world.getDefaultMaterial();

    if (!manager || !material || !m_transform) return;

    //! PxCapsuleControllerDesc の設定
    physx::PxCapsuleControllerDesc desc;
    desc.radius = m_radius;
    desc.height = m_height;
    desc.stepOffset = m_stepOffset;
    desc.slopeLimit = cosf(DirectX::XMConvertToRadians(m_slopeLimitDeg));
    desc.contactOffset = m_contactOffset;
    desc.material = material;
    //desc.reportCallback = this;
    //desc.behaviorCallback = this;

    //! 非歩行モード: スライドを強制して壁で止まらず滑るようにする
    desc.nonWalkableMode = m_slideOnSlopes
        ? physx::PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING
        : physx::PxControllerNonWalkableMode::ePREVENT_CLIMBING;

    //! カプセルのクライミングモード（コンストレイント方式でより正確）
    desc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;

    //! 初期位置を TransformComponent から取得
    Vector3 pos = m_transform->getPosition();
    desc.position = physx::PxExtendedVec3(pos.x, pos.y, pos.z);

    //! up方向はY軸
    desc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);

    if (!desc.isValid())
    {
        LOG_ERROR("CharacterController: PxCapsuleControllerDesc が無効です");
        return;
    }

    m_controller = manager->createController(desc);
    if (!m_controller)
    {
        LOG_ERROR("CharacterController: PxController の作成に失敗しました");
        return;
    }

    //! コントローラーの内部アクターに userData を設定
    physx::PxRigidDynamic* actor = m_controller->getActor();
    if (actor)
    {
        actor->userData = this;
    }

    m_previousPosition = getPosition();

    LOG_INFO("CharacterController 作成完了 (radius=%.2f, height=%.2f)", m_radius, m_height);
}

void CharacterControllerComponent::releaseController()
{
    if (m_controller)
    {
        m_controller->release();
        m_controller = nullptr;
    }
}

CharacterCollisionFlags CharacterControllerComponent::move(const Vector3& displacement)
{
    m_collisionFlags = {};

    if (!m_controller) return m_collisionFlags;

    float dt = TimeManager::Instance().getDeltaTime();
    if (dt <= 0.0f) dt = 1.0f / 60.0f;

    //! 坂道スライドの適用
    Vector3 finalDisplacement = displacement;
    if (m_slideOnSlopes && m_collisionFlags.below)
    {
        applySlopeSliding(finalDisplacement, dt);
    }

    //! PhysX コントローラーの移動フィルター
    physx::PxControllerFilters filters;
    filters.mFilterFlags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC;

    //! PhysX の move を実行
    physx::PxControllerCollisionFlags flags = m_controller->move(
        PhysXHelper::ToPxVec3(finalDisplacement),
        m_minMoveDistance,
        dt,
        filters
    );

    //! 衝突フラグを変換
    m_collisionFlags.below = static_cast<physx::PxU8>(flags & physx::PxControllerCollisionFlag::eCOLLISION_DOWN) != 0;
    m_collisionFlags.above = static_cast<physx::PxU8>(flags & physx::PxControllerCollisionFlag::eCOLLISION_UP) != 0;
    m_collisionFlags.sides = static_cast<physx::PxU8>(flags & physx::PxControllerCollisionFlag::eCOLLISION_SIDES) != 0;

    //! 天井に当たったら垂直速度をリセット
    if (m_collisionFlags.above && m_verticalVelocity > 0.0f)
    {
        m_verticalVelocity = 0.0f;
    }

    return m_collisionFlags;
}

void CharacterControllerComponent::setPosition(const Vector3& position)
{
    if (!m_controller) return;

    m_controller->setPosition(physx::PxExtendedVec3(position.x, position.y, position.z));
    m_previousPosition = position;
    m_verticalVelocity = 0.0f;

    if (m_transform)
    {
        m_transform->setPosition(position);
    }
}

Vector3 CharacterControllerComponent::getPosition() const
{
    if (!m_controller) return m_transform ? m_transform->getPosition() : Vector3::Zero;

    //! PxController::getFootPosition はカプセル底面の位置を返す
    physx::PxExtendedVec3 footPos = m_controller->getFootPosition();
    return Vector3(
        static_cast<float>(footPos.x),
        static_cast<float>(footPos.y),
        static_cast<float>(footPos.z)
    );
}

Vector3 CharacterControllerComponent::getCenterPosition() const
{
    if (!m_controller) return m_transform ? m_transform->getPosition() : Vector3::Zero;

    //! PxController::getPosition はカプセル中心の位置を返す
    physx::PxExtendedVec3 pos = m_controller->getPosition();
    return Vector3(
        static_cast<float>(pos.x),
        static_cast<float>(pos.y),
        static_cast<float>(pos.z)
    );
}

void CharacterControllerComponent::setRadius(float radius)
{
    m_radius = radius;

    if (m_controller)
    {
        auto* capsule = static_cast<physx::PxCapsuleController*>(m_controller);
        capsule->setRadius(radius);
    }
}

void CharacterControllerComponent::setHeight(float height)
{
    m_height = height;

    if (m_controller)
    {
        auto* capsule = static_cast<physx::PxCapsuleController*>(m_controller);
        capsule->setHeight(height);
    }
}

void CharacterControllerComponent::setStepOffset(float offset)
{
    m_stepOffset = offset;

    if (m_controller)
    {
        m_controller->setStepOffset(offset);
    }
}

void CharacterControllerComponent::setSlopeLimit(float degrees)
{
    m_slopeLimitDeg = degrees;

    //! PxController は実行時に slopeLimit を変更できないため、コントローラーを再作成
    if (m_controller)
    {
        Vector3 pos = getPosition();
        releaseController();
        createController();
        setPosition(pos);
    }
}

void CharacterControllerComponent::setContactOffset(float offset)
{
    m_contactOffset = offset;

    if (m_controller)
    {
        m_controller->setContactOffset(offset);
    }
}

void CharacterControllerComponent::applySlopeSliding(Vector3& displacement, float deltaTime)
{
    //! 地面法線から坂の角度を計算
    float slopeAngle = acosf(m_groundNormal.Dot(Vector3::Up));
    float slopeLimitRad = DirectX::XMConvertToRadians(m_slopeLimitDeg);

    //! 歩行可能な角度を超えている場合のみスライドを適用
    if (slopeAngle > slopeLimitRad)
    {
        //! 地面法線の水平成分を取得（滑る方向）
        Vector3 slideDir(m_groundNormal.x, 0.0f, m_groundNormal.z);
        slideDir.Normalize();

        //! 坂の角度に応じたスライド量
        float slideSpeed = sinf(slopeAngle) * m_slideFactor * -PhysicsWorld::Instance().getGravity().y * deltaTime;

        displacement += slideDir * slideSpeed;
    }
}

//=====================================================
// PxUserControllerHitReport コールバック
//=====================================================

//void CharacterControllerComponent::onShapeHit(const physx::PxControllerShapeHit& hit)
//{
//    //! 地面の法線を保存
//    Vector3 hitNormal = PhysXHelper::ToVector3(hit.worldNormal);
//
//    if (hitNormal.y > 0.5f)
//    {
//        m_groundNormal = hitNormal;
//    }
//
//    //! 動的オブジェクトにぶつかった場合、力を加える
//    if (hit.actor && hit.actor->getType() == physx::PxActorType::eRIGID_DYNAMIC)
//    {
//        auto* dynamic = static_cast<physx::PxRigidDynamic*>(hit.actor);
//
//        //! Kinematic は押さない
//        if (dynamic->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)
//            return;
//
//        //! 移動方向に基づいた力を適用
//        Vector3 pushDir = PhysXHelper::ToVector3(hit.dir);
//        pushDir.y = 0.0f;
//
//        float pushMagnitude = m_velocity.Length() * dynamic->getMass() * 0.5f;
//        if (pushMagnitude > 0.0f)
//        {
//            dynamic->addForce(
//                PhysXHelper::ToPxVec3(pushDir * pushMagnitude),
//                physx::PxForceMode::eIMPULSE
//            );
//        }
//    }
//}

//void CharacterControllerComponent::onControllerHit(const physx::PxControllersHit& hit)
//{
//    //! 他のキャラクターコントローラーとの衝突（必要に応じて拡張）
//}
//
//void CharacterControllerComponent::onObstacleHit(const physx::PxControllerObstacleHit& hit)
//{
//    //! 障害物との衝突（PxObstacleContext 使用時に発火）
//}
//
////=====================================================
//// PxControllerBehaviorCallback コールバック
////=====================================================
//
//physx::PxControllerBehaviorFlags CharacterControllerComponent::getBehaviorFlags(
//    const physx::PxShape& shape, const physx::PxActor& actor)
//{
//    //! 動的オブジェクトの上に乗れるようにする
//    if (actor.getType() == physx::PxActorType::eRIGID_DYNAMIC)
//    {
//        return physx::PxControllerBehaviorFlag::eCCT_CAN_RIDE_ON_OBJECT;
//    }
//
//    return physx::PxControllerBehaviorFlags(0);
//}
//
//physx::PxControllerBehaviorFlags CharacterControllerComponent::getBehaviorFlags(
//    const physx::PxController& controller)
//{
//    return physx::PxControllerBehaviorFlags(0);
//}
//
//physx::PxControllerBehaviorFlags CharacterControllerComponent::getBehaviorFlags(
//    const physx::PxObstacle& obstacle)
//{
//    return physx::PxControllerBehaviorFlags(0);
//}