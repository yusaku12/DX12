#include "pch.h"
#include "TestScene.h"
#include "ModelEditorScene.h"

void SceneManager::initialize()
{
    //! シーン登録
    registerScene<TestScene>(SceneId::TEST);
    registerScene<ModelEditorScene>(SceneId::ModelEditor);

    //! 最初のシーンを読み込み
    loadScene(SceneId::TEST);
}

void SceneManager::loadScene(SceneId id)
{
    size_t index = static_cast<size_t>(id);
    assert(m_sceneFactory[index]);

    m_nextScene = m_sceneFactory[index]();
    m_nextSceneID = id;
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
        m_currentScene->drawMultiThreaded();
    }
}

void SceneManager::debugOption()
{
    ImGui::Begin("GameScene");

    //! 現在シーン表示
    ImGui::Text("Current Scene: %s", magic_enum::enum_name(m_currentSceneID).data());

    ImGui::Separator();

    //! enum 全列挙
    for (auto id : magic_enum::enum_values<SceneId>())
    {
        if (id == SceneId::MAX) //!< Countは除外
            continue;

        bool isCurrent = (id == m_currentSceneID);

        const char* name = magic_enum::enum_name(id).data();

        if (isCurrent)
        {
            ImGui::Text("* %s", name);
        }
        else if (ImGui::Button(name))
        {
            loadScene(id);
        }
    }

    ImGui::End();

    //! 現在シーンのデバック描画
    m_currentScene->debugDraw();
}

void SceneManager::changeSceneInternal()
{
    if (m_currentScene)
    {
        m_currentScene->onExit();
    }

    m_currentScene = std::move(m_nextScene);
    m_currentSceneID = m_nextSceneID;

    if (m_currentScene)
    {
        m_currentScene->onEnter();
    }

    m_nextScene.reset();
    m_requestChange = false;
}