#include "pch.h"
#include "PostEffectComponent.h"

PostEffectComponent::PostEffectComponent()
{
}

void PostEffectComponent::awake()
{
}

void PostEffectComponent::update()
{
}

void PostEffectComponent::render()
{
}

void PostEffectComponent::render(ID3D12GraphicsCommandList* cmd)
{
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void PostEffectComponent::inspectGUI()
{
}