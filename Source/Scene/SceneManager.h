#pragma once

#include "Scene.h"
#include "SceneFlatBuffer.h"

//! シーンID
enum class SceneId : int
{
    ModelEditor,
    ParticleEditor,
    MAX
};

//=======================
// SceneManager
//=======================
class SceneManager
{
public:
    using SceneLoadProgressCallback = std::function<void(float normalized, bool done, bool success, bool cancelled, const std::string& message)>;

    //! シングルトン
    static SceneManager& Instance()
    {
        static SceneManager instance;
        return instance;
    }

    //! シーン登録
    template<typename T>
    void registerScene(SceneId id)
    {
        static_assert(std::is_base_of<Scene, T>::value, "T must derive from Scene");

        m_sceneFactory[static_cast<size_t>(id)] = []()
            {
                return DXMem::makeUnique<T>();
            };
    }

    //! 初期化
    void initialize();

    //! 終了処理
    void shutdown();

    //! シーン切り替え
    void loadScene(SceneId id);

    //! 毎フレーム更新（GameLoopから呼ぶ）
    void update();

    //! 描画
    void draw();

    //! デバック機能
    void debugOption();

    //! 現在シーンの実行状態を保存
    bool saveCurrentScene(const std::filesystem::path& filePath) const;

    //! シーンファイルから実行状態を読み込み（次フレーム update で適用）
    bool loadSceneFromFile(const std::filesystem::path& filePath);

    //! バックグラウンドでシーンを読み込み、適用をメインスレッドへ予約
    uint64_t loadSceneFromFileBackground(const std::filesystem::path& filePath,
        int priority,
        SceneLoadProgressCallback progressCallback = {});

    //! バックグラウンドシーン読み込みをキャンセル
    bool cancelBackgroundSceneLoad(uint64_t requestId);

    //! 進行中のバックグラウンドシーン読み込みをすべてキャンセル
    void cancelAllBackgroundSceneLoads();

    //! バックグラウンドシーン読み込み状態
    bool isBackgroundSceneLoadBusy() const;

    //! 現在シーン取得
    SceneId getCurrentSceneID() const { return m_currentSceneID; }

    //! 現在シーンのマルチスレッド描画設定取得
    bool isCurrentSceneMultiThreadedRenderingEnabled() const;

private:

    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    //! 実際のシーン切り替え処理
    void changeSceneInternal();

    //! シーンファイル読み込みの実処理
    bool loadSceneFromFileInternal(const std::filesystem::path& filePath);

    struct BackgroundSceneTask
    {
        uint64_t id = 0;
        std::filesystem::path path;
        int priority = 0;
        uint64_t sequence = 0;
        SceneLoadProgressCallback progressCallback;
    };

    struct BackgroundSceneResult
    {
        BackgroundSceneTask task;
        bool ok = false;
        bool cancelled = false;
        std::string message;
        std::unique_ptr<SceneFlatBuffer::PreparedSceneData> prepared;
    };

    struct BackgroundSceneWorker
    {
        BackgroundSceneTask task;
        std::future<BackgroundSceneResult> future;
    };

    void dispatchBackgroundSceneLoads();
    void pumpBackgroundSceneLoads();
    void applyBackgroundSceneLoads();
    void reportBackgroundProgress(const BackgroundSceneTask& task,
        float normalized,
        bool done,
        bool success,
        bool cancelled,
        std::string_view message) const;
    bool isBackgroundSceneLoadCancelled(uint64_t requestId) const;

    //! メンバ変数
    using SceneFactory = std::function<std::unique_ptr<Scene>()>;
    std::array<SceneFactory, magic_enum::enum_count<SceneId>()> m_sceneFactory{};
    std::unique_ptr<Scene> m_currentScene;
    std::unique_ptr<Scene> m_nextScene;
    SceneId m_currentSceneID = SceneId::ParticleEditor;
    SceneId m_nextSceneID = SceneId::ParticleEditor;
    bool m_requestChange = false;
    bool m_requestLoadSceneFile = false;
    std::filesystem::path m_pendingSceneFilePath;

    uint64_t m_nextBackgroundRequestId = 1;
    uint64_t m_nextBackgroundSequence = 1;
    std::deque<BackgroundSceneTask> m_backgroundScenePending;
    std::vector<BackgroundSceneWorker> m_backgroundSceneWorkers;
    std::deque<BackgroundSceneResult> m_backgroundSceneCompleted;
    std::unordered_set<uint64_t> m_backgroundSceneCancelledIds;

    static constexpr size_t kMaxBackgroundSceneWorkers = 1;
};