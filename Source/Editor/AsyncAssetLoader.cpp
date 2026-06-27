#include "pch.h"
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
        return enqueue(RequestType::Scene, scenePath, nullptr);
    }

    bool AsyncAssetLoader::enqueuePrefab(const std::filesystem::path& prefabPath, GameObject* parent)
    {
        return enqueue(RequestType::Prefab, prefabPath, parent);
    }

    bool AsyncAssetLoader::enqueueFbx(const std::filesystem::path& fbxPath, GameObject* parent)
    {
        return enqueue(RequestType::Fbx, fbxPath, parent);
    }

    void AsyncAssetLoader::update()
    {
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
        m_workers.clear();
        m_completed.clear();
        m_lastStatus.clear();
    }

    bool AsyncAssetLoader::isBusy() const
    {
        return !m_workers.empty() || !m_completed.empty();
    }

    size_t AsyncAssetLoader::pendingTaskCount() const
    {
        return m_workers.size() + m_completed.size();
    }

    bool AsyncAssetLoader::enqueue(RequestType type, const std::filesystem::path& path, GameObject* parent)
    {
        if (path.empty())
        {
            return false;
        }

        Request request;
        request.type = type;
        request.path = normalizePath(path);
        request.parentId = parent ? parent->getInstanceId() : 0;

        Worker worker;
        worker.request = request;
        worker.future = std::async(std::launch::async, [request]()
            {
                return preprocess(request);
            });

        m_workers.push_back(std::move(worker));

        m_lastStatus = std::format("Queued async load: {}", pathToUtf8(request.path));
        return true;
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

    void AsyncAssetLoader::pumpWorkers()
    {
        auto it = m_workers.begin();
        while (it != m_workers.end())
        {
            if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                m_completed.push_back(it->future.get());
                it = m_workers.erase(it);
                continue;
            }

            ++it;
        }
    }

    void AsyncAssetLoader::applyResult(const Result& result)
    {
        const std::string pathText = pathToUtf8(result.request.path);

        if (!result.ok)
        {
            m_lastStatus = std::format("Async load failed: {} ({})", pathText, result.message);
            LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
            return;
        }

        switch (result.request.type)
        {
        case RequestType::Scene:
        {
            if (SceneManager::Instance().loadSceneFromFile(result.request.path))
            {
                g_editor.selectedObject = nullptr;
                m_lastStatus = std::format("Scene load scheduled: {}", pathText);
            }
            else
            {
                m_lastStatus = std::format("Scene load request rejected: {}", pathText);
                LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
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
                break;
            }

            g_editor.selectedObject = instance;
            m_lastStatus = std::format("Prefab loaded: {}", pathText);
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

            GameObject* object = new GameObject(objectName);
            object->addComponent<TransformComponent>();

            const std::string modelPath = pathToUtf8(result.request.path);
            FbxRenderComponent* renderer = object->addComponent<FbxRenderComponent>(modelPath);
            if (!renderer)
            {
                object->destroy();
                m_lastStatus = std::format("FBX component create failed: {}", pathText);
                LOG_ERROR("[AsyncAssetLoader] %s", m_lastStatus.c_str());
                break;
            }

            object->setParent(parent);
            g_editor.selectedObject = object;
            m_lastStatus = std::format("FBX loaded: {}", pathText);
            break;
        }
        default:
            break;
        }
    }
}
