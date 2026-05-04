#include "pch.h"
#include "Component\PostEffectComponent.h"
#include "Camera\CameraComponent.h"

void PostEffectManager::registerComponent(PostEffectComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it == m_components.end())
    {
        m_components.push_back(comp);
    }
}

void PostEffectManager::unregisterComponent(PostEffectComponent* comp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::erase(m_components, comp);
}

UINT PostEffectManager::execute(UINT sceneSrvIndex)
{
    std::vector<PostEffectComponent*> comps;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        comps = m_components;
    }

    if (comps.empty())
        return sceneSrvIndex;

    CameraComponent* cam = CameraManager::Instance().getMainCamera();
    Vector3 cameraPos = cam ? cam->getPosition() : Vector3::Zero;

    std::sort(comps.begin(), comps.end(),
        [](PostEffectComponent* a, PostEffectComponent* b)
        {
            return a->getVolumePriority() < b->getVolumePriority();
        });

    auto& rt = PostEffectRenderTargets::Instance();
    rt.reset(sceneSrvIndex);

    bool any = false;

    for (auto* comp : comps)
    {
        if (!comp) continue;

        float weight = comp->computeBlendWeight(cameraPos);
        if (weight <= 0.0f) continue;

        if (comp->executeChain(weight))
            any = true;
    }

    return any ? rt.getFinalOutputSrvIndex() : sceneSrvIndex;
}