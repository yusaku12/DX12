#include "pch.h"
#include "GameObject/GameObjectRegistry.h"
#include "AsyncAssetLoader.h"

#include "Component/FbxRenderComponent.h"
#include "Component/TransformComponent.h"
#include "EditorContext.h"
#include "GameObject/GameObject.h"
#include "Generated/Prefab_generated.h"
#include "Generated/Scene_generated.h"
#include "Scene/PrefabFlatBuffer.h"
#include "Scene/SceneManager.h"

namespace
{
    std::filesystem::path normalizePath(const std::filesystem::path& source)
    {
        std::error_code ec;
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(source, ec);
        if (!ec)
        {
            return canonical;
        }

        return source.lexically_normal();
    }

    std::string pathToUtf8(const std::filesystem::path& path)
    {
        const std::u8string u8 = path.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }

    std::vector<uint8_t> readBinary(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return {};
        }

        const std::streamsize size = file.tellg();
        if (size <= 0)
        {
            return {};
        }

        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            return {};
        }

        return bytes;
    }

    bool validateSceneFile(const std::filesystem::path& filePath, std::string& outMessage)
    {
        const std::vector<uint8_t> bytes = readBinary(filePath);
        if (bytes.empty())
        {
            outMessage = "Failed to read scene file";
            return false;
        }

        if (!scene::SerializedSceneBufferHasIdentifier(bytes.data()))
        {
            outMessage = "Invalid scene identifier";
            return false;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        if (!scene::VerifySerializedSceneBuffer(verifier))
        {
            outMessage = "Invalid scene flatbuffer";
            return false;
        }

        return true;
    }

    bool validatePrefabFile(const std::filesystem::path& filePath, std::string& outMessage)
    {
        const std::vector<uint8_t> bytes = readBinary(filePath);
        if (bytes.empty())
        {
            outMessage = "Failed to read prefab file";
            return false;
        }

        if (!scene::SerializedPrefabBufferHasIdentifier(bytes.data()))
        {
            outMessage = "Invalid prefab identifier";
            return false;
        }

        flatbuffers::Verifier verifier(bytes.data(), bytes.size());
        if (!scene::VerifySerializedPrefabBuffer(verifier))
        {
            outMessage = "Invalid prefab flatbuffer";
            return false;
        }

        return true;
    }

    bool validateFbxFile(const std::filesystem::path& filePath, std::string& outMessage)
    {
        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec) || ec)
        {
            outMessage = "FBX file not found";
            return false;
        }

        if (!std::filesystem::is_regular_file(filePath, ec) || ec)
        {
            outMessage = "FBX path is not a file";
            return false;
        }

        return true;
    }
}

namespace EditorAsyncAsset
{
    bool AsyncAssetLoader::enqueueScene(const std::filesystem::path& scenePath)
    {
        return enqueueSceneEx(scenePath).accepted;
    }

    bool AsyncAssetLoader::enqueuePrefab(const std::filesystem::path& prefabPath, GameObject* parent)
    {
        return enqueuePrefabEx(prefabPath, parent).accepted;
    }

    bool AsyncAssetLoader::enqueueFbx(const std::filesystem::path& fbxPath, GameObject* parent)
    {
        return enqueueFbxEx(fbxPath, parent).accepted;
    }

    EnqueueResult AsyncAssetLoader::enqueueSceneEx(const std::filesystem::path& scenePath,
        LoadPriority priority,
        ProgressCallback progressCallback)
    {
        return enqueue(RequestType::Scene, scenePath, nullptr, priority, std::move(progressCallback));
    }

    EnqueueResult AsyncAssetLoader::enqueuePrefabEx(const std::filesystem::path& prefabPath,
        GameObject* parent,
        LoadPriority priority,
        ProgressCallback progressCallback)
    {
        return enqueue(RequestType::Prefab, prefabPath, parent, priority, std::move(progressCallback));
    }

    EnqueueResult AsyncAssetLoader::enqueueFbxEx(const std::filesystem::path& fbxPath,
        GameObject* parent,
        LoadPriority priority,
        ProgressCallback progressCallback)
    {
        return enqueue(RequestType::Fbx, fbxPath, parent, priority, std::move(progressCallback));
    }

    bool AsyncAssetLoader::cancel(uint64_t requestId)
    {
        if (requestId == 0)
        {
            return false;
        }

        auto pendingIt = std::find_if(m_pending.begin(), m_pending.end(),
            [requestId](const Request& request) { return request.id == requestId; });

        if (pendingIt != m_pending.end())
        {
            reportProgress(*pendingIt, 1.0f, "Cancelled", true, false, true);
            m_pending.erase(pendingIt);
            m_cancelledIds.insert(requestId);
            return true;
        }

        for (const Worker& worker : m_workers)
        {
            if (worker.request.id == requestId)
            {
                m_cancelledIds.insert(requestId);
                m_lastStatus = std::format("Cancel requested: {}", pathToUtf8(worker.request.path));
                return true;
            }
        }

        return false;
    }

    void AsyncAssetLoader::cancelAll()
    {
        for (const Request& request : m_pending)
        {
            reportProgress(request, 1.0f, "Cancelled", true, false, true);
            m_cancelledIds.insert(request.id);
        }
        m_pending.clear();

        for (const Worker& worker : m_workers)
        {
            m_cancelledIds.insert(worker.request.id);
        }

        for (const Result& result : m_completed)
        {
            m_cancelledIds.insert(result.request.id);
        }

        m_lastStatus = "All async requests marked as cancelled";
    }

    void AsyncAssetLoader::update()
    {
        dispatchPending();
        pumpWorkers();

        constexpr size_t kMaxApplyPerFrame = 2;
        size_t applied = 0;
        while (!m_completed.empty() && applied < kMaxApplyPerFrame)
        {
            Result result = std::move(m_completed.front());
            m_completed.pop_front();
            applyResult(result);
            ++applied;
        }
    }

    void AsyncAssetLoader::clear()
    {
        cancelAll();
        m_workers.clear();
        m_completed.clear();
        m_cancelledIds.clear();
        m_lastStatus.clear();
    }

    bool AsyncAssetLoader::isBusy() const
    {
        return !m_pending.empty() || !m_workers.empty() || !m_completed.empty();
    }

    size_t AsyncAssetLoader::pendingTaskCount() const
    {
        return m_pending.size() + m_workers.size() + m_completed.size();
    }

    float AsyncAssetLoader::getOverallProgress() const
    {
        const size_t activeCount = pendingTaskCount();
        if (activeCount == 0)
        {
            return 1.0f;
        }

        float progress = 0.0f;
        progress += static_cast<float>(m_completed.size()) * 0.9f;
        progress += static_cast<float>(m_workers.size()) * 0.5f;

        return std::clamp(progress / static_cast<float>(activeCount), 0.0f, 0.99f);
    }

    EnqueueResult AsyncAssetLoader::enqueue(RequestType type,
        const std::filesystem::path& path,
        GameObject* parent,
        LoadPriority priority,
        ProgressCallback progressCallback)
    {
        if (path.empty())
        {
            return {};
        }

        Request request;
        request.id = m_nextRequestId++;
        request.type = type;
        request.priority = priority;
        request.path = normalizePath(path);
        request.parentId = parent ? parent->getInstanceId() : 0;
        request.progressCallback = std::move(progressCallback);
        request.sequence = m_nextSequence++;

        m_pending.push_back(request);
        std::stable_sort(m_pending.begin(), m_pending.end(),
            [](const Request& lhs, const Request& rhs)
            {
                if (lhs.priority != rhs.priority)
                {
                    return static_cast<int>(lhs.priority) < static_cast<int>(rhs.priority);
                }

                return lhs.sequence < rhs.sequence;
            });

        reportProgress(request, 0.0f, "Queued");
        m_lastStatus = std::format("Queued async load[#{}]: {}", request.id, pathToUtf8(request.path));

        EnqueueResult result;
        result.accepted = true;
        result.requestId = request.id;
        return result;
    }

    AsyncAssetLoader::Result AsyncAssetLoader::preprocess(Request request)
    {
        Result result;
        result.request = std::move(request);

        std::error_code ec;
        if (!std::filesystem::exists(result.request.path, ec) || ec)
        {
            result.ok = false;
            result.message = "File does not exist";
            return result;
        }

        switch (result.request.type)
        {
        case RequestType::Scene:
            result.ok = validateSceneFile(result.request.path, result.message);
            break;
        case RequestType::Prefab:
            result.ok = validatePrefabFile(result.request.path, result.message);
            break;
        case RequestType::Fbx:
            result.ok = validateFbxFile(result.request.path, result.message);
            break;
        default:
            result.ok = false;
            result.message = "Unknown request type";
            break;
        }

        if (result.ok)
        {
            result.message = "Ready";
        }

        return result;
    }

    void AsyncAssetLoader::dispatchPending()
    {
        while (!m_pending.empty() && m_workers.size() < kMaxWorkers)
        {
            Request request = std::move(m_pending.front());
            m_pending.pop_front();

            if (isCancelled(request.id))
            {
                reportProgress(request, 1.0f, "Cancelled", true, false, true);
                continue;
            }

            reportProgress(request, 0.15f, "Preprocessing");

            Worker worker;
            worker.request = request;
            worker.future = std::async(std::launch::async, [request]()
                {
                    return preprocess(request);
                });
            m_workers.push_back(std::move(worker));
        }
    }

    void AsyncAssetLoader::pumpWorkers()
    {
        auto it = m_workers.begin();
        while (it != m_workers.end())
        {
            if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                Result result = it->future.get();
                if (isCancelled(result.request.id))
                {
                    reportProgress(result.request, 1.0f, "Cancelled", true, false, true);
                }
                else
                {
                    reportProgress(result.request, 0.7f, result.ok ? "Preprocess complete" : "Preprocess failed");
                    m_completed.push_back(std::move(result));
                }
                it = m_workers.erase(it);
                continue;
            }

            ++it;
        }
    }

    void AsyncAssetLoader::applyResult(const Result& result)
    {
        if (isCancelled(result.request.id))
        {
            reportProgress(result.request, 1.0f, "Cancelled", true, false, true);
            return;
        }

        const std::string pathText = pathToUtf8(result.request.path);

        if (!result.ok)
        {
            m_lastStatus = std::format("Async load failed: {} ({})", pathText, result.message);
            LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
            reportProgress(result.request, 1.0f, "Failed", true, false, false);
            return;
        }

        switch (result.request.type)
        {
        case RequestType::Scene:
        {
            int scenePriority = 0;
            switch (result.request.priority)
            {
            case LoadPriority::Critical: scenePriority = 100; break;
            case LoadPriority::High: scenePriority = 75; break;
            case LoadPriority::Normal: scenePriority = 50; break;
            case LoadPriority::Low: scenePriority = 25; break;
            default: break;
            }

            const uint64_t backgroundRequestId = SceneManager::Instance().loadSceneFromFileBackground(
                result.request.path,
                scenePriority,
                [this, request = result.request](float normalized, bool done, bool success, bool cancelled, const std::string& message)
                {
                    reportProgress(request, normalized, message, done, success, cancelled);
                });

            if (backgroundRequestId != 0)
            {
                g_editor.selectedObject = nullptr;
                m_lastStatus = std::format("Scene load scheduled: {}", pathText);
                reportProgress(result.request, 0.8f, "Scheduled", false, false, false);
            }
            else
            {
                m_lastStatus = std::format("Scene load request rejected: {}", pathText);
                LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
                reportProgress(result.request, 1.0f, "Rejected", true, false, false);
            }
            break;
        }
        case RequestType::Prefab:
        {
            GameObject* parent = result.request.parentId == 0
                ? nullptr
                : GameObjectRegistry::Instance().findByInstanceId(result.request.parentId);

            GameObject* instance = PrefabFlatBuffer::instantiate(result.request.path, parent);
            if (!instance)
            {
                m_lastStatus = std::format("Prefab instantiate failed: {}", pathText);
                LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
                reportProgress(result.request, 1.0f, "Instantiate failed", true, false, false);
                break;
            }

            g_editor.selectedObject = instance;
            m_lastStatus = std::format("Prefab loaded: {}", pathText);
            reportProgress(result.request, 1.0f, "Loaded", true, true, false);
            break;
        }
        case RequestType::Fbx:
        {
            GameObject* parent = result.request.parentId == 0
                ? nullptr
                : GameObjectRegistry::Instance().findByInstanceId(result.request.parentId);

            const std::string objectName = result.request.path.stem().string().empty()
                ? "Model"
                : result.request.path.stem().string();

            GameObject* object = DX_NEW(GameObject, objectName);
            object->addComponent<TransformComponent>();

            const std::string modelPath = pathToUtf8(result.request.path);
            FbxRenderComponent* renderer = object->addComponent<FbxRenderComponent>(modelPath);
            if (!renderer)
            {
                object->destroy();
                m_lastStatus = std::format("FBX component create failed: {}", pathText);
                LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
                reportProgress(result.request, 1.0f, "Component create failed", true, false, false);
                break;
            }

            object->setParent(parent);
            g_editor.selectedObject = object;
            m_lastStatus = std::format("FBX loaded: {}", pathText);
            reportProgress(result.request, 1.0f, "Loaded", true, true, false);
            break;
        }
        default:
            break;
        }

        m_cancelledIds.erase(result.request.id);
    }

    bool AsyncAssetLoader::isCancelled(uint64_t requestId) const
    {
        return m_cancelledIds.find(requestId) != m_cancelledIds.end();
    }

    void AsyncAssetLoader::reportProgress(const Request& request,
        float normalized,
        std::string_view stage,
        bool done,
        bool success,
        bool cancelled)
    {
        const float clamped = std::clamp(normalized, 0.0f, 1.0f);

        if (request.progressCallback)
        {
            LoadProgress progress;
            progress.requestId = request.id;
            progress.type = request.type;
            progress.path = request.path;
            progress.normalized = clamped;
            progress.done = done;
            progress.success = success;
            progress.cancelled = cancelled;
            progress.stage = std::string(stage);
            request.progressCallback(progress);
        }

        m_lastStatus = std::format("#{} {} {:.0f}%", request.id, std::string(stage), clamped * 100.0f);
    }
}
