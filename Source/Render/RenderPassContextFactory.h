#pragma once

#include "RenderPassBase.h"

// RenderPassContext creation helper
inline RenderPassContext BuildRenderPassContext(bool useMultiThreaded, UINT sceneSrvIndex)
{
    RenderPassContext context{};
    context.renderPath = CameraManager::Instance().getMainRenderPath();
    context.passMask = CameraManager::Instance().getMainRenderPassMask();
    context.useMultiThreaded = useMultiThreaded;
    context.sceneSrvIndex = sceneSrvIndex;
    context.finalSrvIndex = sceneSrvIndex;
    return context;
}
