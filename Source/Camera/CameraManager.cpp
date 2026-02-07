#include "pch.h"

void CameraManager::setBehaviour(std::unique_ptr<CameraBehaviour> behaviour)
{
    m_behaviour = std::move(behaviour);
}

void CameraManager::initialize()
{
    //! カメラ定数バッファ作成
    m_cameraCB = std::make_unique<ConstantBuffer<GPUCameraBuffer>>();

    //! カメラ定数バッファをGPUにアップロード
    uploadCameraBufferToGPU();
}

void CameraManager::update()
{
    if (m_behaviour)
    {
        m_behaviour->update(m_camera);

        //! カメラ定数バッファをGPUにアップロード
        uploadCameraBufferToGPU();
    }
}

void CameraManager::uploadCameraBufferToGPU()
{
    //! カメラ定数バッファ更新
    GPUCameraBuffer camera{};
    camera.view = m_camera.getView();
    camera.projection = m_camera.getProjection();
    camera.cameraPos = m_camera.getPosition();

    //! 定数バッファ更新(1:シェーダーに登録する番号)
    m_cameraCB->update(1, camera);
}