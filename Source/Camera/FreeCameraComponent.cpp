#include "pch.h"
#include "FreeCameraComponent.h"
#include "Component\TransformComponent.h"

void FreeCameraComponent::start()
{
    // 親クラスの start を先に呼ぶ
    // - TransformComponent をキャッシュ（m_transform へ格納）
    // - CameraManager への登録
    CameraComponent::start();

    if (!m_transform) return;

    // 初期回転（m_yaw=180° を正面向きに）
    Quaternion qYaw   = Quaternion::CreateFromAxisAngle(Vector3::Up,    m_yaw);
    Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, m_pitch);
    m_transform->setRotation(qPitch * qYaw);
}

void FreeCameraComponent::update()
{
    if (!DX12::Instance().isSceneActive()) return;

    // 遅延初期化（start が呼ばれなかった場合のフォールバック）
    if (!m_transform && gameObject())
        m_transform = gameObject()->getComponent<TransformComponent>();

    if (!m_transform) return;

    auto& input = InputManager::Instance();
    float dt     = TimeManager::Instance().getDeltaTime();

    float speedMul = input.isKeyHeld(VK_SHIFT) ? 4.0f : 1.0f;
    float speed    = m_moveSpeed * speedMul * dt;

    // ─── 回転（右マウスドラッグ） ──────────────────────
    if (input.isMouseHeld(1))
    {
        POINT delta = input.getMouseDelta();

        m_yaw   += delta.x * m_mouseSensitivity;
        m_pitch += delta.y * m_mouseSensitivity;

        const float limit = DirectX::XM_PIDIV2 - 0.01f;
        m_pitch = std::clamp(m_pitch, -limit, limit);

        Quaternion qYaw   = Quaternion::CreateFromAxisAngle(Vector3::Up,    m_yaw);
        Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, m_pitch);
        m_transform->setRotation(qPitch * qYaw);
    }

    // ─── 移動（カメラ基準） ────────────────────────────
    Quaternion rot = m_transform->getRotation();
    Vector3 forward = Vector3::Transform(Vector3::Forward, rot);
    Vector3 right   = Vector3::Transform(Vector3::Right,   rot);
    Vector3 up      = Vector3::Transform(Vector3::Up,      rot);

    Vector3 move = Vector3::Zero;
    if (input.isKeyHeld('W')) move += forward;
    if (input.isKeyHeld('S')) move -= forward;
    if (input.isKeyHeld('D')) move += right;
    if (input.isKeyHeld('A')) move -= right;
    if (input.isKeyHeld('E')) move += up;
    if (input.isKeyHeld('Q')) move -= up;

    if (move.LengthSquared() > 0.0f)
    {
        move.Normalize();
        m_transform->translate(move * speed);
    }

    // ─── ホイールズーム ────────────────────────────────
    int wheel = input.getMouseWheel();
    if (wheel != 0)
        m_transform->translate(forward * (wheel * 0.002f));
}

void FreeCameraComponent::inspectGUI()
{
    // 親クラスの項目（FOV・Near・Far・Depth・位置/方向情報）を先に表示
    CameraComponent::inspectGUI();

    ImGui::Separator();

    // フリーカメラ固有の設定
    ImGui::DragFloat("Move Speed",        &m_moveSpeed,        0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Mouse Sensitivity", &m_mouseSensitivity, 0.0001f, 0.0001f, 0.05f);

    ImGui::Separator();

    if (ImGui::Button("Reset Rotation"))
    {
        m_yaw   = DirectX::XMConvertToRadians(180.0f);
        m_pitch = 0.0f;
        if (m_transform)
        {
            Quaternion qYaw   = Quaternion::CreateFromAxisAngle(Vector3::Up,    m_yaw);
            Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, m_pitch);
            m_transform->setRotation(qPitch * qYaw);
        }
    }
}
