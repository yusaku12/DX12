#include "pch.h"
#include "Scene.h"
#include "GameObject\GameObject.h"
#include "Camera/CameraComponent.h"

void Scene::onExit()
{
    // シーン破棄時に全てのゲームオブジェクトを削除
    for (auto& object : GameObjectRegistry::Instance().getAll())
    {
        object->destroy();
    }
}

void Scene::draw()
{
    RenderManager::Instance().setMultiThreadedEnabled(m_useMultiThreadedRendering);

    RenderPassContext context{};
    context.renderPath = CameraManager::Instance().getMainRenderPath();
    context.passMask = CameraManager::Instance().getMainRenderPassMask();
    context.useMultiThreaded = m_useMultiThreadedRendering;
    context.sceneSrvIndex = DX12::Instance().getSceneSrvIndex();
    context.finalSrvIndex = context.sceneSrvIndex;

    RenderPipeline::Instance().execute(context, RenderPassStage::Scene);
}