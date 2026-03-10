#pragma once

#include "CameraBehaviour.h"

//=====================================================
// 自由カメラ挙動クラス
//=====================================================
class FreeCameraBehaviour : public CameraBehaviour
{
public:

    //! コンストラクタ
    FreeCameraBehaviour(Camera& camera)
    {
        m_camera = &camera;

        //! 初期角度設定
        m_yaw = DirectX::XMConvertToRadians(180.0f); //!< 左右回転
        m_pitch = DirectX::XMConvertToRadians(0.0f); //!< 上下回転

        //! 初期回転を反映
        Quaternion qYaw = Quaternion::CreateFromAxisAngle(Vector3::Up, m_yaw);
        Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, m_pitch);

        m_camera->setRotation(qPitch * qYaw);
    }

    //! カメラの更新
    void update() override;

private:

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};