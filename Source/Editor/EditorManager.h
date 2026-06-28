#pragma once

//=====================================================
// Editor 全体を管理するマネージャ
//=====================================================
class EditorManager
{
public:

    //! シングルトンインスタンス取得
    static EditorManager& Instance()
    {
        static EditorManager instance;
        return instance;
    }

    //! 更新（Scene ウィンドウのオブジェクトピッキング処理）
    void update();

    //! ImGui 描画
    void imgui();

    //! Scene ウィンドウ表示状態
    bool isSceneWindowVisible() const { return m_windowState.scene; }

private:

    struct WindowState
    {
        bool scene = true;
        bool hierarchy = true;
        bool inspector = true;
        bool project = true;
        bool debugHub = true;
        bool sceneSettings = false;
    };

    void drawDockspaceHost();
    void drawMainMenuBar();
    void applyDefaultLayout(ImGuiID dockspaceId);
    void drawManagedWindows();
    void drawDebugHubWindow();

    EditorManager() = default;
    ~EditorManager() = default;

    EditorManager(const EditorManager&) = delete;
    EditorManager& operator=(const EditorManager&) = delete;

    WindowState m_windowState;
    bool m_layoutInitialized = false;
    bool m_requestLayoutReset = true;
};