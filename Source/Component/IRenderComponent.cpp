#include "pch.h"
#include "Render/RenderManager.h"
#include "IRenderComponent.h"

void IRenderComponent::start()
{
    RenderManager::Instance().registerComponent(this);
}

void IRenderComponent::onDestroy()
{
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

void IRenderComponent::renderGBuffer(ID3D12GraphicsCommandList* cmd)
{
    render(cmd);
}

void IRenderComponent::renderForward(ID3D12GraphicsCommandList* cmd)
{
    (void)cmd;
}