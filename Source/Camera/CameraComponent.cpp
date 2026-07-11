#include "pch.h"
#include "Camera/CameraManager.h"
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

    const char* renderPathItems[] = { "Deferred", "Forward" };
    int renderPath = static_cast<int>(m_renderPath);
    if (ImGui::Combo("Render Path", &renderPath, renderPathItems, IM_ARRAYSIZE(renderPathItems)))
    {
        m_renderPath = static_cast<RenderPath>(renderPath);
    }

    ImGui::Separator();

    ImGui::Text("Render Passes");

    if (m_renderPath == RenderPath::Deferred)
    {
        bool gbuffer = isRenderPassEnabled(RenderPassFlags::GBuffer);
        bool lighting = isRenderPassEnabled(RenderPassFlags::Lighting);
        bool forward = isRenderPassEnabled(RenderPassFlags::Forward);

        if (ImGui::Checkbox("GBuffer", &gbuffer))
            setRenderPassEnabled(RenderPassFlags::GBuffer, gbuffer);

        if (ImGui::Checkbox("Lighting", &lighting))
            setRenderPassEnabled(RenderPassFlags::Lighting, lighting);

        if (ImGui::Checkbox("Forward (Transparent)", &forward))
            setRenderPassEnabled(RenderPassFlags::Forward, forward);
    }
    else
    {
        bool forward = isRenderPassEnabled(RenderPassFlags::Forward);
        if (ImGui::Checkbox("Forward", &forward))
            setRenderPassEnabled(RenderPassFlags::Forward, forward);
    }

    bool post = isRenderPassEnabled(RenderPassFlags::PostEffect);
    if (ImGui::Checkbox("PostEffect", &post))
        setRenderPassEnabled(RenderPassFlags::PostEffect, post);

    bool debug = isRenderPassEnabled(RenderPassFlags::Debug);
    if (ImGui::Checkbox("Debug", &debug))
        setRenderPassEnabled(RenderPassFlags::Debug, debug);

    bool shadow = isRenderPassEnabled(RenderPassFlags::ShadowMap);
    if (ImGui::Checkbox("ShadowMap", &shadow))
        setRenderPassEnabled(RenderPassFlags::ShadowMap, shadow);

    bool rayTracing = isRenderPassEnabled(RenderPassFlags::RayTracing);
    if (ImGui::Checkbox("RayTracing", &rayTracing))
        setRenderPassEnabled(RenderPassFlags::RayTracing, rayTracing);

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

Matrix CameraComponent::getView() const
{
    Vector3    pos = getPosition();
    Vector3    fwd = getForward();
    Vector3    up = getUp();
    return Matrix::CreateLookAt(pos, pos + fwd, up);
}

Matrix CameraComponent::getProjection() const
{
    float aspect = static_cast<float>(DX12::Instance().getScreenWidth())
        / static_cast<float>(DX12::Instance().getScreenHeight());
    return Matrix::CreatePerspectiveFieldOfView(m_fov, aspect, m_nearZ, m_farZ);
}

Vector3 CameraComponent::getPosition() const
{
    if (m_transform)
        return m_transform->getPosition();
    return Vector3::Zero;
}

Vector3 CameraComponent::getForward() const
{
    return Vector3::Transform(Vector3::Forward, getRotation());
}

Vector3 CameraComponent::getRight() const
{
    return Vector3::Transform(Vector3::Right, getRotation());
}

Vector3 CameraComponent::getUp() const
{
    return Vector3::Transform(Vector3::Up, getRotation());
}

Quaternion CameraComponent::getRotation() const
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