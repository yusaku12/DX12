#pragma once

class CanvasComponent;
class UIButtonComponent;
class GameObject;
struct ImDrawList;
struct ImRect;

class RuntimeUIManager
{
public:

    static RuntimeUIManager& Instance()
    {
        static RuntimeUIManager instance;
        return instance;
    }

    void initialize();
    void shutdown();

    //! 毎フレーム入力処理（Window::update() から呼ぶ）
    void update();

    //! ImGui DrawList を使ったエディタ内 UI 描画（EditorManager から呼ぶ）
    void render();

    //! ネイティブ DX12 描画（Window::render() のシーン描画後に呼ぶ）
    void renderNative();

    void registerCanvas(CanvasComponent* canvas);
    void unregisterCanvas(CanvasComponent* canvas);

    bool wantsMouseCapture() const { return m_wantsMouseCapture; }

private:

    //! マウスヒット検索結果
    struct ButtonHit
    {
        UIButtonComponent* button = nullptr;
        ImRect             rect;
    };

    RuntimeUIManager() = default;
    ~RuntimeUIManager() = default;
    RuntimeUIManager(const RuntimeUIManager&) = delete;
    RuntimeUIManager(RuntimeUIManager&&) = delete;
    RuntimeUIManager& operator=(const RuntimeUIManager&) = delete;
    RuntimeUIManager& operator=(RuntimeUIManager&&) = delete;

    // ── ImGui ベース描画（エディタ専用）────────────────────
    bool   tryGetSceneRect(ImRect& outRect) const;
    bool   tryGetMouseClientPosition(ImVec2& outPosition) const;
    bool   findTopButtonRecursive(GameObject* object, const ImRect& parentRect,
                                  const ImVec2& point, ButtonHit& outHit) const;
    void   drawCanvasRecursive(GameObject* object, const ImRect& parentRect,
                               ImDrawList* drawList) const;
    ImRect resolveObjectRect(GameObject* object, const ImRect& parentRect) const;

    // ── ネイティブ描画────────────────────────────────────
    //! Screen Overlay Canvas を UIRenderer で描画
    void renderNativeCanvas(CanvasComponent* canvas,
                            float screenW, float screenH);

    //! World Space Canvas を UIRenderer で描画
    void renderWorldSpaceCanvas(CanvasComponent* canvas);

    //! オブジェクト以下のウィジェットを再帰的に UIRenderer に投入
    void drawNativeRecursive(GameObject* object,
                             float parentX, float parentY,
                             float parentW, float parentH);

    //! RectTransform による矩形を解決する
    void resolveNativeRect(GameObject* object,
                           float parentX, float parentY,
                           float parentW, float parentH,
                           float& outX,  float& outY,
                           float& outW,  float& outH) const;

    //! マウス座標をスクリーン空間で取得（ネイティブ入力判定用）
    bool tryGetNativeMousePos(float& outX, float& outY) const;

    //! ネイティブモードのボタンヒット検索
    bool findTopButtonNative(GameObject* object,
                             float parentX, float parentY,
                             float parentW, float parentH,
                             float mouseX,  float mouseY,
                             ButtonHit& outHit) const;

    std::vector<CanvasComponent*> m_canvases;
    bool     m_initialized       = false;
    bool     m_wantsMouseCapture = false;
    uint64_t m_hoveredButtonId   = 0;
    uint64_t m_pressedButtonId   = 0;
};