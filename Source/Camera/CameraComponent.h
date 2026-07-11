#pragma once

#include "Component\Component.h"

class TransformComponent;

//=====================================================
//! 描画パス種別
//=====================================================
enum class RenderPath : int
{
    Deferred = 0,
    Forward
};

//=====================================================
//! 描画パスフラグ
//=====================================================
enum class RenderPassFlags : unsigned int
{
    None = 0,
    GBuffer = 1 << 0,
    Lighting = 1 << 1,
    Forward = 1 << 2,
    PostEffect = 1 << 3,
    Debug = 1 << 4,
    ShadowMap = 1 << 5,
    RayTracing = 1 << 6,
};

inline RenderPassFlags operator|(RenderPassFlags a, RenderPassFlags b)
{
    return static_cast<RenderPassFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline RenderPassFlags operator&(RenderPassFlags a, RenderPassFlags b)
{
    return static_cast<RenderPassFlags>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

inline RenderPassFlags& operator|=(RenderPassFlags& a, RenderPassFlags b)
{
    a = a | b;
    return a;
}

inline bool HasRenderPass(RenderPassFlags mask, RenderPassFlags flag)
{
    return (static_cast<unsigned int>(mask) & static_cast<unsigned int>(flag)) != 0;
}

//=====================================================
// カメラコンポーネント（基底クラス）
//=====================================================
class CameraComponent : public Component
{
public:

    CameraComponent() = default;
    ~CameraComponent() override = default;

    //! 初期化
    void awake() override;

    //! ゲーム開始時（TransformComponent キャッシュ & CameraManager 登録）
    void start() override;

    //! 有効化されたとき
    void onEnable() override;

    //! 無効化されたとき
    void onDisable() override;

    //! 破棄される直前
    void onDestroy() override;

    //! インスペクタ表示
    void inspectGUI() override;

    //! 視野角（ラジアン）の取得
    float getFov() const { return m_fov; }

    //! 視野角（ラジアン）の設定
    void setFov(float fov) { m_fov = fov; }

    //! ニアクリップ距離の取得
    float getNear() const { return m_nearZ; }

    //! ニアクリップ距離の設定
    void setNear(float nearZ) { m_nearZ = nearZ; }

    //! ファークリップ距離の取得
    float getFar() const { return m_farZ; }

    //! ファークリップ距離の設定
    void setFar(float farZ) { m_farZ = farZ; }

    //! カメラ優先度の取得（高いほど優先される）
    int getDepth() const { return m_depth; }

    //! カメラ優先度の設定
    void setDepth(int depth) { m_depth = depth; }

    //! 描画パス取得
    RenderPath getRenderPath() const { return m_renderPath; }

    //! 描画パス設定
    void setRenderPath(RenderPath path) { m_renderPath = path; }

    //! 描画パスマスク取得
    RenderPassFlags getRenderPassMask() const { return m_renderPassMask; }

    //! 描画パスマスク設定
    void setRenderPassMask(RenderPassFlags mask) { m_renderPassMask = mask; }

    //! 描画パス有効判定
    bool isRenderPassEnabled(RenderPassFlags flag) const { return HasRenderPass(m_renderPassMask, flag); }

    //! 描画パス有効/無効設定
    void setRenderPassEnabled(RenderPassFlags flag, bool enabled)
    {
        if (enabled)
        {
            m_renderPassMask |= flag;
        }
        else
        {
            m_renderPassMask = static_cast<RenderPassFlags>(
                static_cast<unsigned int>(m_renderPassMask) & ~static_cast<unsigned int>(flag));
        }
    }

    //! ビュー行列の取得
    Matrix getView() const;

    //! プロジェクション行列の取得
    Matrix getProjection() const;

    //! カメラのワールド座標
    Vector3 getPosition() const;

    //! カメラの前方ベクトル
    Vector3 getForward() const;

    //! カメラの右ベクトル
    Vector3 getRight() const;

    //! カメラの上ベクトル
    Vector3 getUp() const;

    //! カメラの回転（クォータニオン）
    Quaternion getRotation() const;

protected:

    //! 同 GameObject の TransformComponent（派生クラスからもアクセス可能）
    TransformComponent* m_transform = nullptr;

private:

    //! CameraManager への登録（start/onEnable で呼ぶ）
    void registerToManager();

    //! CameraManager からの解除（onDisable/onDestroy で呼ぶ）
    void unregisterFromManager();

    float m_fov = DirectX::XM_PIDIV4; //!< 視野角（ラジアン）
    float m_nearZ = 0.1f;               //!< ニアクリップ距離
    float m_farZ = 1000.0f;            //!< ファークリップ距離
    int   m_depth = 0;                  //!< カメラ優先度
    RenderPath m_renderPath = RenderPath::Deferred; //!< 描画パス
    RenderPassFlags m_renderPassMask = RenderPassFlags::GBuffer
        | RenderPassFlags::Lighting
        | RenderPassFlags::Forward
        | RenderPassFlags::PostEffect
        | RenderPassFlags::Debug
        | RenderPassFlags::ShadowMap
        | RenderPassFlags::RayTracing;

    bool m_initialized = false; //!< awake 完了フラグ（onEnable の早期呼び出しを防ぐ）
    bool m_registered = false; //!< CameraManager 登録済みフラグ
};