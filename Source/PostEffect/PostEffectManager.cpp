#include "pch.h"
#include "PostEffect/PostEffectManager.h"
#include "Camera/CameraManager.h"
#include "Component\PostEffectComponent.h"
#include "Camera\CameraComponent.h"

void PostEffectManager::registerComponent(PostEffectComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it == m_components.end())
    {
        m_components.push_back(comp);
        m_dirty = true;
    }
}

void PostEffectManager::unregisterComponent(PostEffectComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t before = m_components.size();
    std::erase(m_components, comp);
    if (m_components.size() != before)
        m_dirty = true;
}

UINT PostEffectManager::execute(UINT sceneSrvIndex)
{
    std::vector<PostEffectComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_dirty)
        {
            m_sortedComponents = m_components;
            std::sort(m_sortedComponents.begin(), m_sortedComponents.end(),
                [](PostEffectComponent* a, PostEffectComponent* b)
                {
                    return a->getVolumePriority() < b->getVolumePriority();
                });
            m_dirty = false;
        }

        comps = m_sortedComponents;
    }

    if (comps.empty())
    {
        return sceneSrvIndex;
    }

    bool needDepth = false;
    for (auto* comp : comps)
    {
        if (comp && comp->requiresDepth())
        {
            needDepth = true;
            break;
        }
    }

    if (needDepth)
        DX12::Instance().transitionDepthToSRV();

    CameraComponent* cam = CameraManager::Instance().getMainCamera();
    Vector3 cameraPos = cam ? cam->getPosition() : Vector3::Zero;

    auto& rt = PostEffectRenderTargets::Instance();
    rt.reset(sceneSrvIndex);

    bool any = false;

    for (auto* comp : comps)
    {
        if (!comp) continue;

        float weight = comp->computeBlendWeight(cameraPos);
        if (weight <= 0.0f) continue;

        if (comp->executeChain(weight))
        {
            any = true;
        }
    }

    if (needDepth)
        DX12::Instance().transitionDepthToWrite();

    return any ? rt.getFinalOutputSrvIndex() : sceneSrvIndex;
}