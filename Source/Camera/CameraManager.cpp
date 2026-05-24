#include "pch.h"
#include "CameraComponent.h"

void CameraManager::registerCamera(CameraComponent* cam)
{
    // 重複登録を防ぐ
    auto it = std::find(m_cameras.begin(), m_cameras.end(), cam);
    if (it == m_cameras.end())
        m_cameras.push_back(cam);
}

void CameraManager::unregisterCamera(CameraComponent* cam)
{
    m_cameras.erase(
        std::remove(m_cameras.begin(), m_cameras.end(), cam),
        m_cameras.end());
}

CameraComponent* CameraManager::getMainCamera() const
{
    CameraComponent* best = nullptr;
    int bestDepth = INT_MIN;
    for (auto* cam : m_cameras)
    {
        if (cam && cam->isActiveInHierarchy() && cam->getDepth() > bestDepth)
        {
            bestDepth = cam->getDepth();
            best = cam;
        }
    }
    return best;
}

RenderPath CameraManager::getMainRenderPath() const
{
    CameraComponent* mainCam = getMainCamera();
    return mainCam ? mainCam->getRenderPath() : RenderPath::Deferred;
}

RenderPassFlags CameraManager::getMainRenderPassMask() const
{
    CameraComponent* mainCam = getMainCamera();
    return mainCam ? mainCam->getRenderPassMask() : RenderPassFlags::None;
}

void CameraManager::initialize()
{
    // カメラ定数バッファ作成
    m_cameraCB = std::make_unique<ConstantBuffer<GPUCameraBuffer>>();

    // カメラ定数バッファをGPUにアップロード
    uploadCameraBufferToGPU();
}

void CameraManager::shutdown()
{
    m_cameras.clear();
    m_cameraCB.reset();
}

void CameraManager::update()
{
    // GPU 定数バッファを更新
    uploadCameraBufferToGPU();
}

void CameraManager::uploadCameraBufferToGPU()
{
    CameraComponent* mainCam = getMainCamera();

    if (!mainCam)
        return;

    auto view = mainCam->getView();
    auto proj = mainCam->getProjection();

    GPUCameraBuffer camera{};
    camera.view = view;
    camera.projection = proj;
    camera.viewProjection = (view * proj);
    camera.viewInverse = view.Invert();
    camera.cameraPos = mainCam->getPosition();

    m_cameraCB->update(camera);
}