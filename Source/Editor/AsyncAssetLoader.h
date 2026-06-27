#pragma once

#include <deque>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

class GameObject;

namespace EditorAsyncAsset
{
    enum class RequestType
    {
        Scene,
        Prefab,
        Fbx,
    };

    class AsyncAssetLoader
    {
    public:
        static AsyncAssetLoader& Instance()
        {
            static AsyncAssetLoader instance;
            return instance;
        }

        bool enqueueScene(const std::filesystem::path& scenePath);
        bool enqueuePrefab(const std::filesystem::path& prefabPath, GameObject* parent = nullptr);
        bool enqueueFbx(const std::filesystem::path& fbxPath, GameObject* parent = nullptr);

        void update();
        void clear();

        bool isBusy() const;
        size_t pendingTaskCount() const;
        const std::string& getLastStatus() const { return m_lastStatus; }

    private:
        AsyncAssetLoader() = default;
        ~AsyncAssetLoader() = default;

        AsyncAssetLoader(const AsyncAssetLoader&) = delete;
        AsyncAssetLoader& operator=(const AsyncAssetLoader&) = delete;

        struct Request
        {
            RequestType type = RequestType::Scene;
            std::filesystem::path path;
            uint64_t parentId = 0;
        };

        struct Result
        {
            Request request;
            bool ok = false;
            std::string message;
        };

        struct Worker
        {
            Request request;
            std::future<Result> future;
        };

        bool enqueue(RequestType type, const std::filesystem::path& path, GameObject* parent);
        static Result preprocess(Request request);
        void pumpWorkers();
        void applyResult(const Result& result);

        std::vector<Worker> m_workers;
        std::deque<Result> m_completed;
        std::string m_lastStatus;
    };
}
