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

    //! ImGui 描画
    void imgui();

private:

    EditorManager() = default;
    ~EditorManager() = default;

    EditorManager(const EditorManager&) = delete;
    EditorManager& operator=(const EditorManager&) = delete;
};