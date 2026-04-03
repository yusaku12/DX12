#include "pch.h"
#include "CameraComponent.h"
#include "Component\TransformComponent.h"

void CameraComponent::awake()
{
    // awake 完了フラグを立てる
    m_initialized = true;
}

void CameraComponent::start()
{
    // TransformComponent をキャッシュ
    if (gameObject())
        m_transform = gameObject()->getComponent<TransformComponent>();

    // 有効な場合のみ CameraManager に登録
    if (isActiveInHierarchy())
        registerToManager();
}

void CameraComponent::onEnable()
{
    // awake 前の早期呼び出しを防ぐ
    if (!m_initialized) return;

    // start が呼ばれなかった場合の遅延初期化
    if (!m_transform && gameObject())
        m_transform = gameObject()->getComponent<TransformComponent>();

    registerToManager();
}

void CameraComponent::onDisable()
{
    unregisterFromManager();
}

void CameraComponent::onDestroy()
{
    unregisterFromManager();
}

void CameraComponent::inspectGUI()
{
    float fovDeg = DirectX::XMConvertToDegrees(m_fov);
    if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.5f, 1.0f, 179.0f))
        m_fov = DirectX::XMConvertToRadians(fovDeg);

    ImGui::DragFloat("Near", &m_nearZ, 0.01f, 0.001f, 10.0f);
    ImGui::DragFloat("Far", &m_farZ, 1.0f, 1.0f, 10000.0f);
    ImGui::DragInt("Depth", &m_depth, 1.0f, -100, 100);

    ImGui::Separator();

    Vector3 pos = getPosition();
    ImGui::Text("Position : %.2f  %.2f  %.2f", pos.x, pos.y, pos.z);

    Vector3 fwd = getForward();
    Vector3 rgt = getRight();
    Vector3 up = getUp();
    ImGui::Text("Forward  : %.2f  %.2f  %.2f", fwd.x, fwd.y, fwd.z);
    ImGui::Text("Right    : %.2f  %.2f  %.2f", rgt.x, rgt.y, rgt.z);
    ImGui::Text("Up       : %.2f  %.2f  %.2f", up.x, up.y, up.z);
}

const Matrix& CameraComponent::getView() const
{
    Vector3    pos = getPosition();
    Vector3    fwd = getForward();
    Vector3    up = getUp();
    return Matrix::CreateLookAt(pos, pos + fwd, up);
}

const Matrix& CameraComponent::getProjection() const
{
    float aspect = static_cast<float>(DX12::Instance().getScreenWidth())
        / static_cast<float>(DX12::Instance().getScreenHeight());
    return Matrix::CreatePerspectiveFieldOfView(m_fov, aspect, m_nearZ, m_farZ);
}

const Vector3& CameraComponent::getPosition() const
{
    if (m_transform)
        return m_transform->getPosition();
    return Vector3::Zero;
}

const Vector3& CameraComponent::getForward() const
{
    return Vector3::Transform(Vector3::Forward, getRotation());
}

const Vector3& CameraComponent::getRight() const
{
    return Vector3::Transform(Vector3::Right, getRotation());
}

const Vector3& CameraComponent::getUp() const
{
    return Vector3::Transform(Vector3::Up, getRotation());
}

const Quaternion& CameraComponent::getRotation() const
{
    if (m_transform)
        return m_transform->getRotation();
    return Quaternion::Identity;
}

void CameraComponent::registerToManager()
{
    if (!m_registered)
    {
        CameraManager::Instance().registerCamera(this);
        m_registered = true;
    }
}

void CameraComponent::unregisterFromManager()
{
    if (m_registered)
    {
        CameraManager::Instance().unregisterCamera(this);
        m_registered = false;
    }
}