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

    void update();
    void render();

    void registerCanvas(CanvasComponent* canvas);
    void unregisterCanvas(CanvasComponent* canvas);

    bool wantsMouseCapture() const { return m_wantsMouseCapture; }

private:

    struct ButtonHit
    {
        UIButtonComponent* button = nullptr;
        ImRect rect;
    };

    RuntimeUIManager() = default;
    ~RuntimeUIManager() = default;

    RuntimeUIManager(const RuntimeUIManager&) = delete;
    RuntimeUIManager(RuntimeUIManager&&) = delete;
    RuntimeUIManager& operator=(const RuntimeUIManager&) = delete;
    RuntimeUIManager& operator=(RuntimeUIManager&&) = delete;

    bool tryGetSceneRect(ImRect& outRect) const;
    bool tryGetMouseClientPosition(ImVec2& outPosition) const;
    bool findTopButtonRecursive(GameObject* object, const ImRect& parentRect, const ImVec2& point, ButtonHit& outHit) const;
    void drawCanvasRecursive(GameObject* object, const ImRect& parentRect, ImDrawList* drawList) const;
    ImRect resolveObjectRect(GameObject* object, const ImRect& parentRect) const;

    std::vector<CanvasComponent*> m_canvases;
    bool m_initialized = false;
    bool m_wantsMouseCapture = false;
    uint64_t m_hoveredButtonId = 0;
    uint64_t m_pressedButtonId = 0;
};