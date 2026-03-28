#include "pch.h"
#include "IRenderComponent.h"

void IRenderComponent::start()
{
    RenderManager::Instance().registerComponent(this);
}

void IRenderComponent::onDestroy()
{
    // シーン破棄時に必ず登録解除
    RenderManager::Instance().unregisterComponent(this);
}

void IRenderComponent::onEnable()
{
    RenderManager::Instance().registerComponent(this);
}

void IRenderComponent::onDisable()
{
    RenderManager::Instance().unregisterComponent(this);
}