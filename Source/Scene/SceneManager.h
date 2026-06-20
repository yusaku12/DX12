#pragma once

#include "Scene.h"

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
                return std::make_unique<T>();
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

    //! シーンファイルから実行状態を読み込み
    bool loadSceneFromFile(const std::filesystem::path& filePath);

    //! 現在シーン取得
    SceneId getCurrentSceneID() const { return m_currentSceneID; }

private:

    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    //! 実際のシーン切り替え処理
    void changeSceneInternal();

    //! メンバ変数
    using SceneFactory = std::function<std::unique_ptr<Scene>()>;
    std::array<SceneFactory, magic_enum::enum_count<SceneId>()> m_sceneFactory{};
    std::unique_ptr<Scene> m_currentScene;
    std::unique_ptr<Scene> m_nextScene;
    SceneId m_currentSceneID = SceneId::ParticleEditor;
    SceneId m_nextSceneID = SceneId::ParticleEditor;
    bool m_requestChange = false;
};