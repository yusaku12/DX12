#include "pch.h"
#include "Scene.h"
#include "GameObject\GameObject.h"

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
    if (m_useMultiThreadedRendering)
    {
        RenderManager::Instance().renderMultiThreaded();
    }
    else
    {
        RenderManager::Instance().render();
    }
}