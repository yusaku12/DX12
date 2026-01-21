#include "pch.h"

void CameraManager::setBehaviour(std::unique_ptr<CameraBehaviour> behaviour)
{
    m_behaviour = std::move(behaviour);
}

void CameraManager::initialize()
{
    //! カメラ定数バッファ作成
    m_cameraCB = ConstantBuffer<GPUCameraBuffer>();

    //! RootSignature 作成
    createRootSignature();

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

    m_cameraCB.update(camera);
}

void CameraManager::createRootSignature()
{
    auto& rsm = RootSignatureManager::Instance();

    //! カメラ定数バッファを b0 に登録
    rsm.addParameterTo(RootSignatureType::Standard, m_cameraCB.createRootParameter(0));
}