#pragma once

#include "Component\Component.h"
#include <PxPhysicsAPI.h>
#include <characterkinematic/PxController.h>
#include <characterkinematic/PxCapsuleController.h>
#include <characterkinematic/PxControllerBehavior.h>

class TransformComponent;

//=====================================================
// 衝突フラグ（PxControllerCollisionFlag のラッパー）
//=====================================================
struct CharacterCollisionFlags
{
    bool below = false;   //!< 地面に接触しているか
    bool above = false;   //!< 天井に接触しているか
    bool sides = false;   //!< 側面（壁）に接触しているか
};

//=====================================================
// CharacterControllerComponent
// - PhysX PxCapsuleController をラップ
// - FPS / TPS のプレイヤー制御に使用
// - Unity の CharacterController 相当
//
// 主な機能:
// ・段差の自動登り（Step Offset）
// ・壁での自動停止とスライド
// ・坂道の滑り制御（Slope Limit）
// ・重力の手動適用
// ・接地判定
//=====================================================
class CharacterControllerComponent : public Component/*,
    public physx::PxUserControllerHitReport,
    public physx::PxControllerBehaviorCallback*/
{
public:

    CharacterControllerComponent() = default;
    ~CharacterControllerComponent() override;

    //! 初期化（TransformComponent の取得）
    void awake() override;

    //! PhysX コントローラー作成
    void start() override;

    //! 毎フレーム：重力の適用と接地判定の更新
    void update() override;

    //! PhysX -> TransformComponent への位置同期
    void lateUpdate() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! 破棄時
    void onDestroy() override;

    //=====================================================
    // 移動
    //=====================================================

    //! 移動ベクトルを指定してキャラクターを動かす
    //! @param displacement 移動量（ワールド空間）
    //! @return 衝突フラグ
    CharacterCollisionFlags move(const Vector3& displacement);

    //! 位置を直接設定（テレポート用）
    void setPosition(const Vector3& position);

    //! 現在位置を取得（カプセル底面の位置）
    Vector3 getPosition() const;

    //! カプセル中心位置を取得
    Vector3 getCenterPosition() const;

    //=====================================================
    // 接地判定
    //=====================================================

    //! 地面に接触しているか
    bool isGrounded() const { return m_collisionFlags.below; }

    //! 最後の move 呼び出しの衝突フラグ
    const CharacterCollisionFlags& getCollisionFlags() const { return m_collisionFlags; }

    //=====================================================
    // 速度
    //=====================================================

    //! 現在の速度（重力を含む）を取得
    Vector3 getVelocity() const { return m_velocity; }

    //! 垂直速度（重力方向の速度）
    float getVerticalVelocity() const { return m_verticalVelocity; }

    //=====================================================
    // パラメータ設定
    //=====================================================

    //! カプセルの半径
    void setRadius(float radius);
    float getRadius() const { return m_radius; }

    //! カプセルの高さ（半径を含まない中央の直線部分）
    void setHeight(float height);
    float getHeight() const { return m_height; }

    //! 自動で登れる段差の高さ
    void setStepOffset(float offset);
    float getStepOffset() const { return m_stepOffset; }

    //! 登れる坂の最大角度（度）
    void setSlopeLimit(float degrees);
    float getSlopeLimit() const { return m_slopeLimitDeg; }

    //! 他のコントローラーやオブジェクトとの最小距離
    void setContactOffset(float offset);
    float getContactOffset() const { return m_contactOffset; }

    //! 重力の使用
    void setUseGravity(bool use) { m_useGravity = use; }
    bool getUseGravity() const { return m_useGravity; }

    //! 重力スケール（1.0 でシーンの重力をそのまま使用）
    void setGravityScale(float scale) { m_gravityScale = scale; }
    float getGravityScale() const { return m_gravityScale; }

    //! 最小移動距離（これ以下の移動は無視される）
    void setMinMoveDistance(float dist) { m_minMoveDistance = dist; }
    float getMinMoveDistance() const { return m_minMoveDistance; }

    //! スライドモード：壁に沿ってスライドするか
    void setSlideOnSlopes(bool slide) { m_slideOnSlopes = slide; }
    bool getSlideOnSlopes() const { return m_slideOnSlopes; }

    //! 坂道の滑り速度係数
    void setSlideFactor(float factor) { m_slideFactor = factor; }
    float getSlideFactor() const { return m_slideFactor; }

    //! PhysX のコントローラー取得
    physx::PxController* getPxController() const { return m_controller; }

    //=====================================================
    // PxUserControllerHitReport コールバック
    //=====================================================

    //void onShapeHit(const physx::PxControllerShapeHit& hit) override;
    //void onControllerHit(const physx::PxControllersHit& hit) override;
    //void onObstacleHit(const physx::PxControllerObstacleHit& hit) override;

    ////=====================================================
    //// PxControllerBehaviorCallback コールバック
    ////=====================================================

    //physx::PxControllerBehaviorFlags getBehaviorFlags(const physx::PxShape& shape,
    //    const physx::PxActor& actor) override;
    //physx::PxControllerBehaviorFlags getBehaviorFlags(const physx::PxController& controller) override;
    //physx::PxControllerBehaviorFlags getBehaviorFlags(const physx::PxObstacle& obstacle) override;

private:

    //! PhysX コントローラーの作成
    void createController();

    //! PhysX コントローラーの破棄
    void releaseController();

    //! 坂道のスライド処理
    void applySlopeSliding(Vector3& displacement, float deltaTime);

    TransformComponent* m_transform = nullptr;

    physx::PxController* m_controller = nullptr;

    //! カプセル形状パラメータ
    float m_radius = 0.3f;
    float m_height = 1.0f;

    //! 段差の高さ
    float m_stepOffset = 0.3f;

    //! 登れる坂の最大角度（度）
    float m_slopeLimitDeg = 45.0f;

    //! 接触オフセット（スキン幅）
    float m_contactOffset = 0.08f;

    //! 最小移動距離
    float m_minMoveDistance = 0.001f;

    //! 重力関連
    bool m_useGravity = true;
    float m_gravityScale = 1.0f;
    float m_verticalVelocity = 0.0f;

    //! 坂道スライド
    bool m_slideOnSlopes = true;
    float m_slideFactor = 1.0f;

    //! 衝突フラグ
    CharacterCollisionFlags m_collisionFlags;

    //! 速度追跡（前フレーム位置との差分）
    Vector3 m_velocity = Vector3::Zero;
    Vector3 m_previousPosition = Vector3::Zero;

    //! 最後に接触した地面の法線
    Vector3 m_groundNormal = Vector3::Up;
};