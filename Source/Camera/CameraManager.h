#pragma once

//=====================================================
// カメラマネージャ(シングルトン)
//=====================================================
class CameraManager
{
public:

    //static CameraManager& Instance()
    //{
    //    static CameraManager inst;
    //    return inst;
    //}

    //Camera camera;

    //void SetBehaviour(std::unique_ptr<CameraBehaviour> b)
    //{
    //    behaviour = std::move(b);
    //}

    //void Update(float dt)
    //{
    //    if (behaviour)
    //        behaviour->Update(camera, dt);
    //}

private:
    //std::unique_ptr<CameraBehaviour> behaviour;
};