#pragma once

#include "CameraComponent.h"

//=====================================================
// フリーカメラコンポーネント（CameraComponent を継承）
// - CameraComponent の機能（FOV・Near/Far・CameraManager 登録）を持ちつつ、
//   WASD + 右マウスドラッグによる自由視点操作を追加する
// - このコンポーネント1つを addComponent するだけで動作する
//   （CameraComponent を別途 addComponent する必要はない）
// - 同じ GameObject に TransformComponent が必要
//=====================================================
class FreeCameraComponent : public CameraComponent
{
public:

    FreeCameraComponent() = default;
    ~FreeCameraComponent() override = default;

    //! 親クラスの start（Transform キャッシュ & CameraManager 登録）を呼んだ後、初期回転を設定
    void start() override;

    //! 毎フレーム入力処理
    void update() override;

    //! インスペクタ表示（親クラスの項目 + 速度・感度設定）
    void inspectGUI() override;

private:

    float m_yaw              = DirectX::XMConvertToRadians(180.0f); //!< 水平角（左右）
    float m_pitch            = 0.0f;                                //!< 垂直角（上下）
    float m_moveSpeed        = 8.0f;                                //!< 移動速度
    float m_mouseSensitivity = 0.0025f;                             //!< マウス感度
};
