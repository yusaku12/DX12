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
    virtual void update(Camera& camera) = 0;
};