#pragma once

#include <deque>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

class GameObject;

namespace EditorAsyncAsset
{
    enum class LoadPriority : uint8_t
    {
        Critical = 0,
        High,
        Normal,
        Low,
    };

    enum class RequestType
    {
        Scene,
        Prefab,
        Fbx,
    };

    struct LoadProgress
    {
        uint64_t requestId = 0;
        RequestType type = RequestType::Scene;
        std::filesystem::path path;
        float normalized = 0.0f;
        bool done = false;
        bool success = false;
        bool cancelled = false;
        std::string stage;
    };

    using ProgressCallback = std::function<void(const LoadProgress&)>;

    struct EnqueueResult
    {
        bool accepted = false;
        uint64_t requestId = 0;

        explicit operator bool() const { return accepted; }
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

        EnqueueResult enqueueSceneEx(const std::filesystem::path& scenePath,
            LoadPriority priority = LoadPriority::Normal,
            ProgressCallback progressCallback = {});
        EnqueueResult enqueuePrefabEx(const std::filesystem::path& prefabPath,
            GameObject* parent = nullptr,
            LoadPriority priority = LoadPriority::Normal,
            ProgressCallback progressCallback = {});
        EnqueueResult enqueueFbxEx(const std::filesystem::path& fbxPath,
            GameObject* parent = nullptr,
            LoadPriority priority = LoadPriority::Normal,
            ProgressCallback progressCallback = {});

        bool cancel(uint64_t requestId);
        void cancelAll();

        void update();
        void clear();

        bool isBusy() const;
        size_t pendingTaskCount() const;
        float getOverallProgress() const;
        const std::string& getLastStatus() const { return m_lastStatus; }

    private:
        AsyncAssetLoader() = default;
        ~AsyncAssetLoader() = default;

        AsyncAssetLoader(const AsyncAssetLoader&) = delete;
        AsyncAssetLoader& operator=(const AsyncAssetLoader&) = delete;

        struct Request
        {
            uint64_t id = 0;
            RequestType type = RequestType::Scene;
            LoadPriority priority = LoadPriority::Normal;
            std::filesystem::path path;
            uint64_t parentId = 0;
            ProgressCallback progressCallback;
            uint64_t sequence = 0;
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

        EnqueueResult enqueue(RequestType type,
            const std::filesystem::path& path,
            GameObject* parent,
            LoadPriority priority,
            ProgressCallback progressCallback);
        static Result preprocess(Request request);
        void dispatchPending();
        void pumpWorkers();
        void applyResult(const Result& result);
        bool isCancelled(uint64_t requestId) const;
        void reportProgress(const Request& request,
            float normalized,
            std::string_view stage,
            bool done = false,
            bool success = false,
            bool cancelled = false);

        static constexpr size_t kMaxWorkers = 2;

        uint64_t m_nextRequestId = 1;
        uint64_t m_nextSequence = 1;
        std::deque<Request> m_pending;
        std::vector<Worker> m_workers;
        std::deque<Result> m_completed;
        std::unordered_set<uint64_t> m_cancelledIds;
        std::string m_lastStatus;
    };
}
