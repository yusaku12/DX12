#pragma once

#include "Component\Component.h"

class TransformComponent;

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

    //! ビュー行列の取得
    const Matrix& getView() const;

    //! プロジェクション行列の取得
    const Matrix& getProjection() const;

    //! カメラのワールド座標
    const Vector3& getPosition() const;

    //! カメラの前方ベクトル
    const Vector3& getForward() const;

    //! カメラの右ベクトル
    const Vector3& getRight() const;

    //! カメラの上ベクトル
    const Vector3& getUp() const;

    //! カメラの回転（クォータニオン）
    const Quaternion& getRotation() const;

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

    bool m_initialized = false; //!< awake 完了フラグ（onEnable の早期呼び出しを防ぐ）
    bool m_registered = false; //!< CameraManager 登録済みフラグ
};
