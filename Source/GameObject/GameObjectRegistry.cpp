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
    //! 未start のオブジェクトに対して start を呼ぶ
    for (auto* obj : m_objects)
    {
        if (!obj->isDestroyed() && obj->isEnabled() && !obj->isStarted())
        {
            obj->start();
        }
    }

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

    //! 削除予約オブジェクトの破棄
    destroyMarkedObjects();
}

void GameObjectRegistry::destroyMarkedObjects()
{
    auto it = m_objects.begin();
    while (it != m_objects.end())
    {
        GameObject* obj = *it;
        if (obj->isDestroyed())
        {
            delete obj;
            it = m_objects.erase(it);
        }
        else
        {
            ++it;
        }
    }
}