#include "pch.h"
#include "FreeCameraBehaviour.h"
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

void CameraManager::setBehaviour(std::unique_ptr<CameraBehaviour> behaviour)
{
    m_behaviour = std::move(behaviour);
}

void CameraManager::initialize()
{
    // デフォルトでフリーカメラ挙動を設定（フォールバック用）
    setBehaviour(std::make_unique<FreeCameraBehaviour>(m_fallbackCamera));

    // カメラ定数バッファ作成
    m_cameraCB = std::make_unique<ConstantBuffer<GPUCameraBuffer>>();

    // カメラ定数バッファをGPUにアップロード
    uploadCameraBufferToGPU();
}

void CameraManager::update()
{
    CameraComponent* mainCam = getMainCamera();

    if (!mainCam)
    {
        // フォールバック: 従来の CameraBehaviour を更新
        if (m_behaviour)
            m_behaviour->update();
    }

    // GPU 定数バッファを更新（メインカメラまたはフォールバックのデータを使用）
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

    if (mainCam)
    {
        // getName() はコンポーネントの型名を返す（例: "FreeCameraComponent"）
        ImGui::Text("[%s]  depth=%d  cameras=%d",
            mainCam->getName().c_str(),
            mainCam->getDepth(),
            static_cast<int>(m_cameras.size()));
        ImGui::Separator();

        // 登録済みカメラ一覧
        if (m_cameras.size() > 1)
        {
            ImGui::Text("Registered cameras:");
            for (int i = 0; i < static_cast<int>(m_cameras.size()); ++i)
            {
                auto* cam = m_cameras[i];
                bool isMain = (cam == mainCam);
                ImGui::Text("  [%d] %s  depth=%d %s",
                    i,
                    cam->getName().c_str(),
                    cam->getDepth(),
                    isMain ? "<-- main" : "");
            }
            ImGui::Separator();
        }

        // CameraComponent::inspectGUI() に委譲して重複を排除
        mainCam->inspectGUI();
    }
    else
    {
        ImGui::Text("[Fallback Camera]");
        ImGui::Separator();

        Vector3 pos = m_fallbackCamera.getPosition();
        ImGui::Text("Position: %.2f %.2f %.2f", pos.x, pos.y, pos.z);

        Vector3 f = m_fallbackCamera.getForward();
        Vector3 r = m_fallbackCamera.getRight();
        Vector3 u = m_fallbackCamera.getUp();
        ImGui::Text("Forward : %.2f %.2f %.2f", f.x, f.y, f.z);
        ImGui::Text("Right   : %.2f %.2f %.2f", r.x, r.y, r.z);
        ImGui::Text("Up      : %.2f %.2f %.2f", u.x, u.y, u.z);

        ImGui::Separator();

        ImGui::DragFloat3("Edit Position", &m_fallbackCamera.getPosition().x, 0.1f);
        ImGui::DragFloat("FOV",  &m_fallbackCamera.getFov(),  0.01f, 0.1f,  3.0f);
        ImGui::DragFloat("Near", &m_fallbackCamera.getNear(), 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Far",  &m_fallbackCamera.getFar(),  1.0f,  10.0f, 5000.0f);

        if (ImGui::Button("Reset Camera"))
        {
            m_fallbackCamera.setPosition({ 0, 0, -5 });
            m_fallbackCamera.setRotation(Quaternion::Identity);
        }
    }

    ImGui::End();
}

DirectX::SimpleMath::Matrix CameraManager::getView() const
{
    if (CameraComponent* cam = getMainCamera())
        return cam->getView();
    return m_fallbackCamera.getView();
}

DirectX::SimpleMath::Matrix CameraManager::getProjection() const
{
    if (CameraComponent* cam = getMainCamera())
        return cam->getProjection();
    return m_fallbackCamera.getProjection();
}

void CameraManager::uploadCameraBufferToGPU()
{
    auto view = getView();
    auto proj = getProjection();

    GPUCameraBuffer camera{};
    camera.view          = view.Transpose();
    camera.projection    = proj.Transpose();
    camera.viewProjection = (view * proj);

    if (CameraComponent* cam = getMainCamera())
        camera.cameraPos = cam->getPosition();
    else
        camera.cameraPos = m_fallbackCamera.getPosition();

    m_cameraCB->update(camera);
}
