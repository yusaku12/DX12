#pragma once

#include "Camera.h"

//=====================================================
// カメラ挙動基底クラス
//=====================================================
class CameraBehaviour
{
public:

    virtual ~CameraBehaviour() = default;

    //! カメラの更新
    virtual void update() = 0;

protected:

    Camera* m_camera = nullptr;
};