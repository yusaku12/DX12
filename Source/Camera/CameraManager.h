#pragma once

#include "Camera.h"
#include "CameraBehaviour.h"

//=====================================================
// カメラマネージャ(シングルトン)
//=====================================================
class CameraManager
{
public:

    //! シングルトンインスタンス取得
    static CameraManager& Instance()
    {
        static CameraManager inst;
        return inst;
    }

    //! カメラ取得
    Camera m_camera;

    //! カメラ挙動設定
    void setBehaviour(std::unique_ptr<CameraBehaviour> behaviour);

    //! カメラ更新
    void update();

private:
    std::unique_ptr<CameraBehaviour> m_behaviour;
};