#include "pch.h"
#include "NavAgentComponent.h"

#include "TransformComponent.h"
#include "System/NavMeshSystem.h"

namespace
{
    constexpr float kArrivalEpsilon = 0.001f;
    constexpr float kFixedStepSeconds = 1.0f / 60.0f;
}

void NavAgentComponent::awake()
{
    m_transform = gameObject() ? gameObject()->getComponent<TransformComponent>() : nullptr;
}

void NavAgentComponent::update()
{
    if (!m_transform || !m_hasDestination)
    {
        return;
    }

    const float stepDt = kFixedStepSeconds;

    m_repathTimer -= stepDt;
    if (m_repathTimer <= 0.0f)
    {
        m_repathTimer = m_repathInterval;
        rebuildPath();
    }

    if (!hasPath())
    {
        return;
    }

    while (m_pathCursor < m_path.size())
    {
        const Vector3 currentPos = m_transform->getPosition();
        const Vector3 toWaypoint = m_path[m_pathCursor] - currentPos;
        const float distance = toWaypoint.Length();

        if (distance <= m_stoppingDistance + kArrivalEpsilon)
        {
            ++m_pathCursor;
            continue;
        }

        const Vector3 direction = toWaypoint / distance;
        const float step = std::max(0.0f, std::min(m_moveSpeed * stepDt, distance));
        m_transform->setPosition(currentPos + direction * step);
        return;
    }

    const float remainingToDestination = (m_destination - m_transform->getPosition()).Length();
    if (remainingToDestination <= m_stoppingDistance)
    {
        stop();
    }
}

void NavAgentComponent::inspectGUI()
{
    ImGui::Text("NavAgent");

    char pathBuffer[260]{};
    if (!m_navMeshAssetPath.empty())
    {
        const size_t copySize = std::min(m_navMeshAssetPath.size(), sizeof(pathBuffer) - 1);
        std::memcpy(pathBuffer, m_navMeshAssetPath.data(), copySize);
        pathBuffer[copySize] = '\0';
    }

    if (ImGui::InputText("NavMesh Asset", pathBuffer, sizeof(pathBuffer)))
    {
        setNavMeshAssetPath(pathBuffer);
    }

    ImGui::DragFloat("Move Speed", &m_moveSpeed, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Acceleration", &m_acceleration, 0.1f, 0.0f, 200.0f);
    ImGui::DragFloat("Stopping Distance", &m_stoppingDistance, 0.01f, 0.01f, 20.0f);
    ImGui::DragFloat("Repath Interval", &m_repathInterval, 0.01f, 0.05f, 5.0f);

    if (ImGui::Button("Repath"))
    {
        requestRepath();
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        stop();
    }

    if (m_hasDestination)
    {
        ImGui::Text("Destination: %.2f %.2f %.2f", m_destination.x, m_destination.y, m_destination.z);
    }
}

void NavAgentComponent::setDestination(const Vector3& destination)
{
    m_destination = destination;
    m_hasDestination = true;
    requestRepath();
}

void NavAgentComponent::stop()
{
    m_hasDestination = false;
    m_path.clear();
    m_pathCursor = 0;
}

void NavAgentComponent::requestRepath()
{
    m_repathTimer = 0.0f;
}

void NavAgentComponent::setNavMeshAssetPath(const std::string& path)
{
    m_navMeshAssetPath = path;
    if (!m_navMeshAssetPath.empty())
    {
        NavMeshSystem::Instance().loadNavMesh(std::filesystem::path(m_navMeshAssetPath));
    }
}

bool NavAgentComponent::rebuildPath()
{
    m_path.clear();
    m_pathCursor = 0;

    if (!m_transform || !m_hasDestination)
    {
        return false;
    }

    if (!m_navMeshAssetPath.empty() && NavMeshSystem::Instance().getLoadedAssetPath() != std::filesystem::path(m_navMeshAssetPath))
    {
        NavMeshSystem::Instance().loadNavMesh(std::filesystem::path(m_navMeshAssetPath));
    }

    if (!NavMeshSystem::Instance().findPath(m_transform->getPosition(), m_destination, m_path))
    {
        return false;
    }

    while (m_pathCursor < m_path.size())
    {
        const float distance = (m_path[m_pathCursor] - m_transform->getPosition()).Length();
        if (distance > m_stoppingDistance)
        {
            break;
        }
        ++m_pathCursor;
    }

    return true;
}
