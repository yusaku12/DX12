#pragma once

#include "Scene.h"

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
    void registerScene(const std::string& name)
    {
        static_assert(std::is_base_of<Scene, T>::value, "T must derive from Scene");
        m_sceneNames.push_back(name);
        m_sceneFactory[name] = []()
            {
                return std::make_unique<T>();
            };
    }

    //! シーン切り替え
    void loadScene(const std::string& name);

    //! 毎フレーム更新（GameLoopから呼ぶ）
    void update();

    //! 描画
    void draw();

    //! デバック機能
    void debugOption();

    //! 現在のシーン名取得
    const std::string& getCurrentSceneName() const { return m_currentSceneName; }

private:

    SceneManager() = default;
    ~SceneManager() = default;

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    //! 実際のシーン切り替え処理
    void changeSceneInternal();

    //! 登録済みシーン名一覧取得（デバッグ用）
    const std::vector<std::string>& getRegisteredSceneNames() const { return m_sceneNames; }

    //! メンバ変数
    using SceneFactory = std::unique_ptr<Scene>(*)(); //!< シーン生成関数型
    std::unordered_map<std::string, SceneFactory> m_sceneFactory; //!< シーン生成関数マップ
    std::unique_ptr<Scene> m_currentScene; //!< 現在のシーン
    std::unique_ptr<Scene> m_nextScene;    //!< 次のシーン
    std::string m_currentSceneName;        //!< 現在のシーン名
    std::string m_nextSceneName;           //!< 次のシーン名
    std::vector<std::string> m_sceneNames; //!< 登録済みシーン名一覧（デバッグ用）
    bool m_requestChange = false;          //!< シーン切り替え要求フラグ
};
