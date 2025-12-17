#include "pch.h"

void CameraManager::setBehaviour(std::unique_ptr<CameraBehaviour> behaviour)
{
    m_behaviour = std::move(behaviour);
}

void CameraManager::update()
{
    if (m_behaviour)
    {
        m_behaviour->update(m_camera);
    }
}