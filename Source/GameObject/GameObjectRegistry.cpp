#include "pch.h"
#include "GameObject.h"

void GameObjectRegistry::registryGameObject(GameObject* obj)
{
    m_objects.push_back(obj);
}

void GameObjectRegistry::unregister(GameObject* obj)
{
    m_objects.erase(
        std::remove(m_objects.begin(), m_objects.end(), obj),
        m_objects.end()
    );
}

void GameObjectRegistry::update()
{
    //! 全オブジェクトの更新
    for (auto* obj : m_objects)
    {
        obj->update();
    }

    //! 全オブジェクトの後処理更新
    for (auto* obj : m_objects)
    {
        obj->lateUpdate();
    }
}