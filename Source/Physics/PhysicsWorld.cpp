#include "pch.h"
#include "PhysXHelper.h"
#include "Component\RigidbodyComponent.h"

void PhysicsWorld::initialize(const physx::PxVec3& gravity)
{
    if (m_initialized) return;

    // Foundation 作成
    m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback);
    if (!m_foundation)
    {
        LOG_ERROR("PhysX Foundation の作成に失敗しました");
        return;
    }

    // PVD (PhysX Visual Debugger) 接続（デバッグ用）
#ifdef _DEBUG
    m_pvd = physx::PxCreatePvd(*m_foundation);
    physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    if (transport)
    {
        m_pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
    }
#endif

    // Physics 作成
    m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation,
        physx::PxTolerancesScale(), true, m_pvd);
    if (!m_physics)
    {
        LOG_ERROR("PxPhysics の作成に失敗しました");
        return;
    }

    // Extensions 初期化
    PxInitExtensions(*m_physics, m_pvd);

    // CookingParams 初期化（PhysX 5.x では PxCooking オブジェクトは廃止）
    m_cookingParams = physx::PxCookingParams(m_physics->getTolerancesScale());

    // CPU Dispatcher 作成（スレッド数はハードウェアに合わせる）
    UINT numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    m_dispatcher = physx::PxDefaultCpuDispatcherCreate(numThreads);

    // Scene 作成
    physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
    sceneDesc.gravity = gravity;
    sceneDesc.cpuDispatcher = m_dispatcher;
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    sceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;

    m_scene = m_physics->createScene(sceneDesc);
    if (!m_scene)
    {
        LOG_ERROR("PxScene の作成に失敗しました");
        return;
    }

    // PVD クライアント設定
#ifdef _DEBUG
    physx::PxPvdSceneClient* pvdClient = m_scene->getScenePvdClient();
    if (pvdClient)
    {
        pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }
#endif

    // デフォルトマテリアル作成（staticFriction=0.5, dynamicFriction=0.5, restitution=0.3）
    m_defaultMaterial = m_physics->createMaterial(0.5f, 0.5f, 0.3f);

    // コントローラーマネージャー作成
    m_controllerManager = PxCreateControllerManager(*m_scene);
    if (m_controllerManager)
    {
        // 障害物の重複防止モードを有効化
        m_controllerManager->setOverlapRecoveryModule(true);
    }

    m_initialized = true;
    LOG_INFO("PhysX 5.x complete (threads: %u)", numThreads);
}

void PhysicsWorld::shutdown()
{
    if (!m_initialized) return;

    PxCloseExtensions();

    if (m_controllerManager) { m_controllerManager->release(); m_controllerManager = nullptr; }
    if (m_scene) { m_scene->release();     m_scene = nullptr; }
    if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
    if (m_physics) { m_physics->release();   m_physics = nullptr; }

    if (m_pvd)
    {
        physx::PxPvdTransport* transport = m_pvd->getTransport();
        m_pvd->release();
        m_pvd = nullptr;
        if (transport) transport->release();
    }

    if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }

    m_initialized = false;
    LOG_INFO("PhysX ワールド終了");
}

void PhysicsWorld::simulate(float deltaTime)
{
    if (!m_initialized || !m_scene) return;

    m_accumulator += deltaTime;

    int steps = 0;
    while (m_accumulator >= m_fixedTimeStep && steps < m_maxSubSteps)
    {
        m_scene->simulate(m_fixedTimeStep);
        m_scene->fetchResults(true);

        // シミュレーション結果を TransformComponent に反映
        syncTransforms();

        ++steps;
        m_accumulator -= m_fixedTimeStep;
    }
}

void PhysicsWorld::syncTransforms()
{
    if (!m_scene) return;

    // アクティブなアクターを取得して Transform に書き戻す
    physx::PxU32 numActiveActors = 0;
    physx::PxActor** activeActors = m_scene->getActiveActors(numActiveActors);

    for (physx::PxU32 i = 0; i < numActiveActors; ++i)
    {
        auto* actor = activeActors[i];
        if (actor->getType() != physx::PxActorType::eRIGID_DYNAMIC) continue;

        auto* rigid = static_cast<physx::PxRigidDynamic*>(actor);
        auto* rb = static_cast<RigidbodyComponent*>(rigid->userData);
        if (!rb) continue;

        // PhysX のトランスフォームを TransformComponent に反映
        rb->syncFromPhysics();
    }
}

bool PhysicsWorld::raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit) const
{
    if (!m_initialized || !m_scene) return false;

    physx::PxRaycastBuffer hit;
    bool status = m_scene->raycast(
        PhysXHelper::ToPxVec3(origin),
        PhysXHelper::ToPxVec3(direction),
        maxDistance,
        hit
    );

    if (status && hit.hasBlock)
    {
        outHit.point = PhysXHelper::ToVector3(hit.block.position);
        outHit.normal = PhysXHelper::ToVector3(hit.block.normal);
        outHit.distance = hit.block.distance;

        if (hit.block.actor && hit.block.actor->userData)
        {
            outHit.rigidbody = static_cast<RigidbodyComponent*>(hit.block.actor->userData);
        }
        return true;
    }
    return false;
}

bool PhysicsWorld::raycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, std::vector<RaycastHit>& outHits) const
{
    if (!m_initialized || !m_scene) return false;

    constexpr physx::PxU32 MAX_HITS = 64;
    physx::PxRaycastHit hitBuffer[MAX_HITS];
    physx::PxRaycastBuffer buf(hitBuffer, MAX_HITS);

    bool status = m_scene->raycast(
        PhysXHelper::ToPxVec3(origin),
        PhysXHelper::ToPxVec3(direction),
        maxDistance,
        buf
    );

    if (!status) return false;

    outHits.clear();
    for (physx::PxU32 i = 0; i < buf.nbTouches; ++i)
    {
        RaycastHit rh;
        rh.point = PhysXHelper::ToVector3(buf.touches[i].position);
        rh.normal = PhysXHelper::ToVector3(buf.touches[i].normal);
        rh.distance = buf.touches[i].distance;
        if (buf.touches[i].actor && buf.touches[i].actor->userData)
        {
            rh.rigidbody = static_cast<RigidbodyComponent*>(buf.touches[i].actor->userData);
        }
        outHits.push_back(rh);
    }
    return !outHits.empty();
}

void PhysicsWorld::setGravity(const Vector3& gravity)
{
    if (m_scene) m_scene->setGravity(PhysXHelper::ToPxVec3(gravity));
}

Vector3 PhysicsWorld::getGravity() const
{
    if (!m_scene) return Vector3(0.0f, -9.81f, 0.0f);
    return PhysXHelper::ToVector3(m_scene->getGravity());
}