#include "pch.h"
#include "Scene/SceneManager.h"
#include "Editor/AsyncAssetLoader.h"
#include "ModelEditorScene.h"
#include "ParticleScene.h"
#include "SceneFlatBuffer.h"

namespace
{
    std::string pathToUtf8(const std::filesystem::path& path)
    {
        const std::u8string u8 = path.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }
}

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
    m_requestLoadSceneFile = false;
    m_pendingSceneFilePath.clear();
    cancelAllBackgroundSceneLoads();
    m_backgroundTaskExecutor.wait_for_all();
    m_backgroundScenePending.clear();
    {
        std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
        m_backgroundSceneCompleted.clear();
        m_backgroundSceneActiveIds.clear();
    }
    m_backgroundSceneCancelledIds.clear();
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
    dispatchBackgroundSceneLoads();
    applyBackgroundSceneLoads();

    if (m_requestLoadSceneFile)
    {
        const std::filesystem::path pendingPath = m_pendingSceneFilePath;
        m_requestLoadSceneFile = false;
        m_pendingSceneFilePath.clear();

        if (!loadSceneFromFileInternal(pendingPath))
        {
            LOG_ERROR("[SceneManager] Failed to load scene file: %s", pendingPath.string().c_str());
        }
    }

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
    ImGui::Begin("Scene Settings");

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
            EditorAsyncAsset::AsyncAssetLoader::Instance().enqueueScene(std::filesystem::path(paths.front()));
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
    if (filePath.empty())
    {
        return false;
    }

    return loadSceneFromFileBackground(filePath, 0) != 0;
}

uint64_t SceneManager::loadSceneFromFileBackground(const std::filesystem::path& filePath,
    int priority,
    SceneLoadProgressCallback progressCallback)
{
    if (filePath.empty())
    {
        return 0;
    }

    BackgroundSceneTask task;
    task.id = m_nextBackgroundRequestId++;
    task.path = filePath;
    task.priority = priority;
    task.sequence = m_nextBackgroundSequence++;
    task.progressCallback = std::move(progressCallback);

    m_backgroundScenePending.push_back(task);
    std::stable_sort(m_backgroundScenePending.begin(), m_backgroundScenePending.end(),
        [](const BackgroundSceneTask& lhs, const BackgroundSceneTask& rhs)
        {
            if (lhs.priority != rhs.priority)
            {
                return lhs.priority > rhs.priority;
            }
            return lhs.sequence < rhs.sequence;
        });

    reportBackgroundProgress(task, 0.0f, false, false, false, "Queued");
    return task.id;
}

bool SceneManager::cancelBackgroundSceneLoad(uint64_t requestId)
{
    if (requestId == 0)
    {
        return false;
    }

    auto pendingIt = std::find_if(m_backgroundScenePending.begin(), m_backgroundScenePending.end(),
        [requestId](const BackgroundSceneTask& task)
        {
            return task.id == requestId;
        });

    if (pendingIt != m_backgroundScenePending.end())
    {
        reportBackgroundProgress(*pendingIt, 1.0f, true, false, true, "Cancelled");
        m_backgroundScenePending.erase(pendingIt);
        m_backgroundSceneCancelledIds.insert(requestId);
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
        if (m_backgroundSceneActiveIds.find(requestId) != m_backgroundSceneActiveIds.end())
        {
            m_backgroundSceneCancelledIds.insert(requestId);
            BackgroundSceneTask task;
            task.id = requestId;
            reportBackgroundProgress(task, 1.0f, false, false, true, "Cancel requested");
            return true;
        }
    }

    return false;
}

void SceneManager::cancelAllBackgroundSceneLoads()
{
    for (const BackgroundSceneTask& task : m_backgroundScenePending)
    {
        m_backgroundSceneCancelledIds.insert(task.id);
        reportBackgroundProgress(task, 1.0f, true, false, true, "Cancelled");
    }
    m_backgroundScenePending.clear();

    std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
    for (uint64_t requestId : m_backgroundSceneActiveIds)
    {
        m_backgroundSceneCancelledIds.insert(requestId);
    }

    for (const BackgroundSceneResult& result : m_backgroundSceneCompleted)
    {
        m_backgroundSceneCancelledIds.insert(result.task.id);
    }
}

bool SceneManager::isBackgroundSceneLoadBusy() const
{
    std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
    return !m_backgroundScenePending.empty()
        || !m_backgroundSceneActiveIds.empty()
        || !m_backgroundSceneCompleted.empty();
}

bool SceneManager::loadSceneFromFileInternal(const std::filesystem::path& filePath)
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

void SceneManager::dispatchBackgroundSceneLoads()
{
    while (true)
    {
        BackgroundSceneTask task;
        {
            std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
            if (m_backgroundScenePending.empty() || m_backgroundSceneActiveIds.size() >= kMaxBackgroundSceneWorkers)
            {
                break;
            }

            task = std::move(m_backgroundScenePending.front());
            m_backgroundScenePending.pop_front();
            m_backgroundSceneActiveIds.insert(task.id);
        }

        if (isBackgroundSceneLoadCancelled(task.id))
        {
            std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
            m_backgroundSceneActiveIds.erase(task.id);
            reportBackgroundProgress(task, 1.0f, true, false, true, "Cancelled");
            continue;
        }

        reportBackgroundProgress(task, 0.2f, false, false, false, "Preparing");

        m_backgroundTaskExecutor.silent_async([this, task]()
            {
                BackgroundSceneResult result;
                result.task = task;

                auto prepared = DXMem::makeUnique<SceneFlatBuffer::PreparedSceneData>();
                std::string errorMessage;
                result.ok = SceneFlatBuffer::prepareLoad(task.path, *prepared, &errorMessage);
                if (result.ok)
                {
                    result.prepared = std::move(prepared);
                    result.message = "Prepared";
                }
                else
                {
                    result.message = errorMessage.empty() ? "Prepare failed" : errorMessage;
                }

                std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
                m_backgroundSceneCompleted.push_back(std::move(result));
                m_backgroundSceneActiveIds.erase(task.id);
            });
    }
}

void SceneManager::applyBackgroundSceneLoads()
{
    BackgroundSceneResult result;
    {
        std::lock_guard<std::mutex> lock(m_backgroundSceneMutex);
        if (m_backgroundSceneCompleted.empty())
        {
            return;
        }

        result = std::move(m_backgroundSceneCompleted.front());
        m_backgroundSceneCompleted.pop_front();
    }

    if (isBackgroundSceneLoadCancelled(result.task.id))
    {
        reportBackgroundProgress(result.task, 1.0f, true, false, true, "Cancelled");
        return;
    }

    if (!result.ok || !result.prepared)
    {
        reportBackgroundProgress(result.task, 1.0f, true, false, false, result.message);
        LOG_ERROR("[SceneManager] Background scene prepare failed: %s", result.message.c_str());
        return;
    }

    SceneId fileSceneId = m_currentSceneID;
    const bool applied = SceneFlatBuffer::loadPrepared(*result.prepared, m_currentSceneID, &fileSceneId);
    if (!applied)
    {
        reportBackgroundProgress(result.task, 1.0f, true, false, false, "Apply failed");
        LOG_ERROR("[SceneManager] Failed to apply prepared scene: %s", pathToUtf8(result.task.path).c_str());
        return;
    }

    if (fileSceneId >= SceneId::ModelEditor && fileSceneId < SceneId::MAX)
    {
        m_currentSceneID = fileSceneId;
    }

    reportBackgroundProgress(result.task, 1.0f, true, true, false, "Applied");
    m_backgroundSceneCancelledIds.erase(result.task.id);
}

void SceneManager::reportBackgroundProgress(const BackgroundSceneTask& task,
    float normalized,
    bool done,
    bool success,
    bool cancelled,
    std::string_view message) const
{
    if (task.progressCallback)
    {
        task.progressCallback(
            std::clamp(normalized, 0.0f, 1.0f),
            done,
            success,
            cancelled,
            std::string(message));
    }
}

bool SceneManager::isBackgroundSceneLoadCancelled(uint64_t requestId) const
{
    return m_backgroundSceneCancelledIds.find(requestId) != m_backgroundSceneCancelledIds.end();
}

bool SceneManager::isCurrentSceneMultiThreadedRenderingEnabled() const
{
    if (!m_currentScene)
    {
        return true;
    }

    return m_currentScene->isMultiThreadedRenderingEnabled();
}