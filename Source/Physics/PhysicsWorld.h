#pragma once

#define _SILENCE_CXX20_CISO646_REMOVED_WARNING
#include <PxPhysicsAPI.h>
#pragma comment(lib, "PhysX_64.lib")
#pragma comment(lib, "PhysXCommon_64.lib")
#pragma comment(lib, "PhysXCooking_64.lib")
#pragma comment(lib, "PhysXExtensions_static_64.lib")
#pragma comment(lib, "PhysXFoundation_64.lib")
#pragma comment(lib, "PhysXPvdSDK_static_64.lib")
#pragma comment(lib, "PhysXTask_static_64.lib")
#pragma comment(lib, "SceneQuery_static_64.lib")
#pragma comment(lib, "SimulationController_static_64.lib")

class RigidbodyComponent;

//=====================================================
// PhysX をラップするシングルトン物理ワールド
// - PhysX 5.x の Foundation / Physics / Scene を管理
// - Unity の Physics クラス相当
//=====================================================
class PhysicsWorld
{
public:

    //! シングルトンインスタンス取得
    static PhysicsWorld& Instance()
    {
        static PhysicsWorld instance;
        return instance;
    }

    //! 初期化（アプリ起動時に1回呼ぶ）
    void initialize(const physx::PxVec3& gravity = physx::PxVec3(0.0f, -9.81f, 0.0f));

    //! 終了処理
    void shutdown();

    //! 物理シミュレーションを1ステップ進める（内部で固定タイムステップ + fetchResults + Transform同期）
    void simulate(float deltaTime);

    //! PhysX オブジェクトへのアクセス
    physx::PxPhysics* getPhysics() const { return m_physics; }
    physx::PxScene* getScene() const { return m_scene; }

    //! Cooking パラメータ取得（PhysX 5.x では PxPhysics に統合済み）
    const physx::PxCookingParams& getCookingParams() const { return m_cookingParams; }

    //! デフォルトマテリアル（staticFriction, dynamicFriction, restitution）
    physx::PxMaterial* getDefaultMaterial() const { return m_defaultMaterial; }

    //! RaycastHit 情報
    struct RaycastHit
    {
        Vector3 point = Vector3::Zero;
        Vector3 normal = Vector3::Zero;
        float distance = 0.0f;
        RigidbodyComponent* rigidbody = nullptr;
    };

    //! レイキャスト（最も近いヒットを返す）
    bool raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit) const;

    //! レイキャスト（全ヒットを返す）
    bool raycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, std::vector<RaycastHit>& outHits) const;

    //! 重力の設定・取得
    void setGravity(const Vector3& gravity);
    Vector3 getGravity() const;

private:

    PhysicsWorld() = default;
    ~PhysicsWorld() = default;
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    //! シミュレーション結果を RigidbodyComponent に反映（simulate 内部から呼ばれる）
    void syncTransforms();

    physx::PxDefaultAllocator m_allocator;
    physx::PxDefaultErrorCallback m_errorCallback;

    physx::PxFoundation* m_foundation = nullptr;
    physx::PxPhysics* m_physics = nullptr;
    physx::PxScene* m_scene = nullptr;
    physx::PxMaterial* m_defaultMaterial = nullptr;
    physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;
    physx::PxPvd* m_pvd = nullptr;

    //! PhysX 5.x では Cooking はオブジェクトではなくパラメータのみ保持
    physx::PxCookingParams m_cookingParams = physx::PxCookingParams(physx::PxTolerancesScale());

    bool m_initialized = false;

    //! 固定タイムステップ用
    float m_fixedTimeStep = 1.0f / 60.0f;
    float m_accumulator = 0.0f;
    int m_maxSubSteps = 8;
};