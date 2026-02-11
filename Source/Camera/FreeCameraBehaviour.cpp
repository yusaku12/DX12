#include "pch.h"
#include "FreeCameraBehaviour.h"

void FreeCameraBehaviour::update(Camera& camera)
{
    auto& input = InputManager::Instance();
    float dt = TimeManager::Instance().getDeltaTime();

    constexpr float baseSpeed = 8.0f;
    constexpr float sensitivity = 0.0025f;

    float speedMul = input.isKeyHeld(VK_SHIFT) ? 4.0f : 1.0f;
    float speed = baseSpeed * speedMul * dt;

    //! 回転（右ドラッグ）
    if (input.isMouseHeld(1)) //!< 右ボタン
    {
        POINT delta = input.getMouseDelta();

        m_yaw += delta.x * sensitivity;
        m_pitch += delta.y * sensitivity;

        const float limit = DirectX::XM_PIDIV2 - 0.01f;
        m_pitch = std::clamp(m_pitch, -limit, limit);

        Quaternion qYaw = Quaternion::CreateFromAxisAngle(Vector3::Up, m_yaw);
        Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, m_pitch);

        camera.setRotation(qPitch * qYaw);
    }

    //! 移動（カメラ基準）
    Vector3 move = Vector3::Zero;

    if (input.isKeyHeld('W')) move += camera.getForward();
    if (input.isKeyHeld('S')) move -= camera.getForward();
    if (input.isKeyHeld('D')) move += camera.getRight();
    if (input.isKeyHeld('A')) move -= camera.getRight();
    if (input.isKeyHeld('E')) move += camera.getUp();
    if (input.isKeyHeld('Q')) move -= camera.getUp();

    if (move.LengthSquared() > 0)
    {
        move.Normalize();
        camera.setPosition(camera.getPosition() + move * speed);
    }

    //! ホイールドリー
    int wheel = input.getMouseWheel();
    if (wheel != 0)
    {
        camera.setPosition(
            camera.getPosition() +
            camera.getForward() * (wheel * 0.002f)
        );
    }
}