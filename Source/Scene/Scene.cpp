#include "pch.h"
#include "Render/RenderManager.h"
#include "GameObject/GameObjectRegistry.h"
#include "Scene.h"
#include "GameObject\GameObject.h"
#include "Camera/CameraComponent.h"
#include "Render/RenderPassContextFactory.h"

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

    RenderPassContext context = BuildRenderPassContext(
        m_useMultiThreadedRendering,
        DX12::Instance().getSceneSrvIndex());

    RenderPipeline::Instance().execute(context, RenderPassStage::Scene);
}