#include "pch.h"
#include "CameraManager.h"
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

void CameraManager::initialize()
{
    // カメラ定数バッファ作成
    //m_cameraCB = std::make_unique<ConstantBuffer<GPUCameraBuffer>>();

    // カメラ定数バッファをGPUにアップロード
    uploadCameraBufferToGPU();
}

void CameraManager::update()
{
    // GPU 定数バッファを更新
    uploadCameraBufferToGPU();
}

void CameraManager::debugImgui()
{
    if (!ImGui::Begin("Camera"))
    {
        ImGui::End();
        return;
    }

    CameraComponent* mainCam = getMainCamera();

    ImGui::Text("[CameraComponent] depth=%d", mainCam->getDepth());
    ImGui::Separator();

    Vector3 pos = mainCam->getPosition();
    ImGui::Text("Position: %.2f %.2f %.2f", pos.x, pos.y, pos.z);

    Vector3 f = mainCam->getForward();
    Vector3 r = mainCam->getRight();
    Vector3 u = mainCam->getUp();
    ImGui::Text("Forward : %.2f %.2f %.2f", f.x, f.y, f.z);
    ImGui::Text("Right   : %.2f %.2f %.2f", r.x, r.y, r.z);
    ImGui::Text("Up      : %.2f %.2f %.2f", u.x, u.y, u.z);

    ImGui::Separator();

    float fovDeg = DirectX::XMConvertToDegrees(mainCam->getFov());
    if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.5f, 1.0f, 179.0f))
        mainCam->setFov(DirectX::XMConvertToRadians(fovDeg));

    float nearZ = mainCam->getNear();
    if (ImGui::DragFloat("Near", &nearZ, 0.01f, 0.001f, 10.0f))
        mainCam->setNear(nearZ);

    float farZ = mainCam->getFar();
    if (ImGui::DragFloat("Far", &farZ, 1.0f, 1.0f, 10000.0f))
        mainCam->setFar(farZ);

    ImGui::End();
}

void CameraManager::uploadCameraBufferToGPU()
{
    auto view = getMainCamera()->getView();
    auto proj = getMainCamera()->getProjection();

    GPUCameraBuffer camera{};
    camera.view = view.Transpose();
    camera.projection = proj.Transpose();
    camera.viewProjection = (view * proj);

    if (CameraComponent* cam = getMainCamera())
    {
        camera.cameraPos = cam->getPosition();
    }

    m_cameraCB->update(camera);
}