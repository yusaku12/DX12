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
    }

    //! カメラの更新
    void update() override;

private:

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};