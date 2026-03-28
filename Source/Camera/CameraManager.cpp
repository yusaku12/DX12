#include "pch.h"
#include "FreeCameraBehaviour.h"

void CameraManager::setBehaviour(std::unique_ptr<CameraBehaviour> behaviour)
{
    m_behaviour = std::move(behaviour);
}

void CameraManager::initialize()
{
    // デフォルトでフリーカメラ挙動を設定
    setBehaviour(std::make_unique<FreeCameraBehaviour>(m_camera));

    // カメラ定数バッファ作成
    m_cameraCB = std::make_unique<ConstantBuffer<GPUCameraBuffer>>();

    // カメラ定数バッファをGPUにアップロード
    uploadCameraBufferToGPU();
}

void CameraManager::update()
{
    if (m_behaviour)
    {
        m_behaviour->update();

        // カメラ定数バッファをGPUにアップロード
        uploadCameraBufferToGPU();
    }
}

void CameraManager::debugImgui()
{
    if (!ImGui::Begin("Camera"))
    {
        ImGui::End();
        return;
    }

    Vector3 pos = m_camera.getPosition();
    ImGui::Text("Position: %.2f %.2f %.2f", pos.x, pos.y, pos.z);

    Vector3 f = m_camera.getForward();
    Vector3 r = m_camera.getRight();
    Vector3 u = m_camera.getUp();

    ImGui::Separator();
    ImGui::Text("Forward : %.2f %.2f %.2f", f.x, f.y, f.z);
    ImGui::Text("Right   : %.2f %.2f %.2f", r.x, r.y, r.z);
    ImGui::Text("Up      : %.2f %.2f %.2f", u.x, u.y, u.z);

    ImGui::Separator();

    ImGui::DragFloat3("Edit Position", &m_camera.getPosition().x, 0.1f);

    ImGui::DragFloat("FOV", &m_camera.getFov(), 0.01f, 0.1f, 3.0f);
    ImGui::DragFloat("Near", &m_camera.getNear(), 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far", &m_camera.getFar(), 1.0f, 10.0f, 5000.0f);

    if (ImGui::Button("Reset Camera"))
    {
        m_camera.setPosition({ 0,0,-5 });
        m_camera.setRotation(Quaternion::Identity);
    }

    ImGui::End();
}

void CameraManager::uploadCameraBufferToGPU()
{
    auto view = m_camera.getView();
    auto proj = m_camera.getProjection();

    GPUCameraBuffer camera{};
    camera.view = view.Transpose();
    camera.projection = proj.Transpose();
    camera.viewProjection = (view * proj);
    camera.cameraPos = m_camera.getPosition();

    // 定数バッファ更新
    m_cameraCB->update(camera);
}