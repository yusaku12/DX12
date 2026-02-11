#pragma once

#include "CameraBehaviour.h"

//=====================================================
// 自由カメラ挙動クラス
//=====================================================
class FreeCameraBehaviour : public CameraBehaviour
{
public:

    //! カメラの更新
    void update(Camera& camera) override;

private:

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};