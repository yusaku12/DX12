#include "pch.h"
#include "ModelEditorScene.h"
#include "ParticleScene.h"
#include "SceneFlatBuffer.h"

void SceneManager::initialize()
{
    // シーン登録
    registerScene<ParticleScene>(SceneId::ParticleEditor);
    registerScene<ModelEditorScene>(SceneId::ModelEditor);

    // 最初のシーンを読み込み
    loadScene(SceneId::ModelEditor);
}

void SceneManager::shutdown()
{
    if (m_currentScene)
    {
        m_currentScene->onExit();
    }

    m_currentScene.reset();
    m_nextScene.reset();
    m_requestChange = false;
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
    // シーン切り替え要求があればここで反映
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
        m_currentScene->debugDraw();
    }
}

void SceneManager::debugOption()
{
    ImGui::Begin("GameScene");

    // 現在シーン表示
    ImGui::Text("Current Scene: %s", magic_enum::enum_name(m_currentSceneID).data());

    ImGui::Separator();

    if (ImGui::Button("Save Scene (.scn)"))
    {
        std::wstring outPath;
        if (Dialog::saveFile(outPath, L"Save Scene", L"", L"scn") == DialogResult::OK && !outPath.empty())
        {
            saveCurrentScene(std::filesystem::path(outPath));
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Load Scene (.scn)"))
    {
        std::vector<std::wstring> paths;
        if (Dialog::openFile(paths, L"Load Scene", L"", false) == DialogResult::OK && !paths.empty())
        {
            loadSceneFromFile(std::filesystem::path(paths.front()));
        }
    }

    ImGui::Separator();

    // enum 全列挙
    for (auto id : magic_enum::enum_values<SceneId>())
    {
        if (id == SceneId::MAX) // Countは除外
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

    // 現在シーンのデバック描画
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

bool SceneManager::saveCurrentScene(const std::filesystem::path& filePath) const
{
    return SceneFlatBuffer::save(filePath, m_currentSceneID);
}

bool SceneManager::loadSceneFromFile(const std::filesystem::path& filePath)
{
    SceneId fileSceneId = m_currentSceneID;
    const bool loaded = SceneFlatBuffer::load(filePath, m_currentSceneID, &fileSceneId);
    if (!loaded)
    {
        return false;
    }

    // ファイルの SceneId が現在と異なる場合は次回保存時に保持できるよう現在IDだけ更新する
    if (fileSceneId >= SceneId::ModelEditor && fileSceneId < SceneId::MAX)
    {
        m_currentSceneID = fileSceneId;
    }

    return true;
}

bool SceneManager::isCurrentSceneMultiThreadedRenderingEnabled() const
{
    if (!m_currentScene)
    {
        return true;
    }

    return m_currentScene->isMultiThreadedRenderingEnabled();
}