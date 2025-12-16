#include "pch.h"

void SceneManager::loadScene(const std::string& name)
{
    auto it = m_sceneFactory.find(name);
    assert(it != m_sceneFactory.end());

    //! 次のシーンを生成（即切り替えせず一旦保持）
    m_nextScene = it->second();
    m_nextSceneName = name;
    m_requestChange = true;
}

void SceneManager::update()
{
    //! シーン切り替え要求があればここで反映
    if (m_requestChange)
    {
        changeSceneInternal();
    }

    if (m_currentScene)
    {
        m_currentScene->update();
    }
}

void SceneManager::draw()
{
    if (m_currentScene)
    {
        m_currentScene->draw();
    }
}

void SceneManager::debugOption()
{
    ImGui::Begin("Scene Debug");

    ImGui::Text("Current Scene: %s", m_currentSceneName.c_str());
    ImGui::Separator();

    for (const auto& name : m_sceneNames)
    {
        bool isCurrent = (name == m_currentSceneName);
        if (isCurrent)
        {
            ImGui::Text("* %s", name.c_str());
        }
        else if (ImGui::Button(name.c_str()))
        {
            loadScene(name);
        }
    }

    ImGui::End();
}

void SceneManager::changeSceneInternal()
{
    if (m_currentScene)
    {
        m_currentScene->onExit();
    }

    m_currentScene = std::move(m_nextScene);
    m_currentSceneName = m_nextSceneName;

    if (m_currentScene)
    {
        m_currentScene->onEnter();
    }

    m_nextScene.reset();
    m_nextSceneName.clear();
    m_requestChange = false;
}