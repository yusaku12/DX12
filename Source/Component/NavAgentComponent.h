#pragma once

#include "Component.h"

class TransformComponent;

class NavAgentComponent : public Component
{
public:

    NavAgentComponent() = default;
    ~NavAgentComponent() override = default;

    void awake() override;
    void update() override;
    void inspectGUI() override;

    void setDestination(const Vector3& destination);
    bool hasDestination() const { return m_hasDestination; }
    bool hasPath() const { return !m_path.empty() && m_pathCursor < m_path.size(); }
    void stop();
    void requestRepath();

    void setNavMeshAssetPath(const std::string& path);
    const std::string& getNavMeshAssetPath() const { return m_navMeshAssetPath; }

    void setMoveSpeed(float value) { m_moveSpeed = std::max(value, 0.0f); }
    float getMoveSpeed() const { return m_moveSpeed; }

    void setAcceleration(float value) { m_acceleration = std::max(value, 0.0f); }
    float getAcceleration() const { return m_acceleration; }

    void setStoppingDistance(float value) { m_stoppingDistance = std::max(value, 0.01f); }
    float getStoppingDistance() const { return m_stoppingDistance; }

    void setRepathInterval(float value) { m_repathInterval = std::max(value, 0.05f); }
    float getRepathInterval() const { return m_repathInterval; }

    const Vector3& getDestination() const { return m_destination; }

private:

    bool rebuildPath();

    TransformComponent* m_transform = nullptr;
    Vector3 m_destination = Vector3::Zero;
    bool m_hasDestination = false;

    std::vector<Vector3> m_path;
    size_t m_pathCursor = 0;

    float m_moveSpeed = 5.0f;
    float m_acceleration = 24.0f;
    float m_stoppingDistance = 0.2f;
    float m_repathInterval = 0.35f;
    float m_repathTimer = 0.0f;

    std::string m_navMeshAssetPath;
};
