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

private:

    float m_yaw = DirectX::XMConvertToRadians(180.0f); //!< 水平角（左右）
    float m_pitch = 0.0f;                              //!< 垂直角（上下）
    float m_moveSpeed = 8.0f;                          //!< 移動速度
    float m_mouseSensitivity = 0.0025f;                //!< マウス感度
};
