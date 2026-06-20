#pragma once

#include "CameraComponent.h"

//=====================================================
// フリーカメラコンポーネント
//=====================================================
class FreeCameraComponent : public CameraComponent
{
public:

    FreeCameraComponent() = default;
    ~FreeCameraComponent() override = default;

    //! start で TransformComponent をキャッシュし初期回転を設定
    void start() override;

    //! 毎フレーム入力処理
    void update() override;

    //! インスペクタ表示（カメラプロパティ＋フリーカメラ設定）
    void inspectGUI() override;

    //! 保存・復元用パラメータ
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }
    float getMoveSpeed() const { return m_moveSpeed; }
    float getMouseSensitivity() const { return m_mouseSensitivity; }
    void setYaw(float yaw) { m_yaw = yaw; }
    void setPitch(float pitch) { m_pitch = pitch; }
    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

private:

    float m_yaw = DirectX::XMConvertToRadians(180.0f); //!< 水平角（左右）
    float m_pitch = 0.0f;                              //!< 垂直角（上下）
    float m_moveSpeed = 8.0f;                          //!< 移動速度
    float m_mouseSensitivity = 0.0025f;                //!< マウス感度
};
