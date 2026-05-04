#pragma once

class IRenderComponent;

//=====================================================
// RenderManager
// - IRenderComponent の登録・解除
// - シングルスレッド / マルチスレッド描画を一元管理
//=====================================================
class RenderManager
{
public:

    //! シングルトンインスタンス取得
    static RenderManager& Instance()
    {
        static RenderManager instance;
        return instance;
    }

    //! 描画コンポーネント登録
    void registerComponent(IRenderComponent* comp);

    //! 描画コンポーネント登録解除
    void unregisterComponent(IRenderComponent* comp);

    //! シングルスレッド描画（全登録コンポーネントを描画）
    void render();

    //! GBuffer 描画（シングルスレッド）
    void renderGBuffer();

    //! Forward 描画（シングルスレッド）
    void renderForward();

    //! マルチスレッド描画（全登録コンポーネントを並列でコマンド記録）
    void renderMultiThreaded();

    //! GBuffer 描画（マルチスレッド）
    void renderMultiThreadedGBuffer();

    //! Forward 描画（マルチスレッド）
    void renderMultiThreadedForward();

    //! マルチスレッド使用フラグ設定
    void setMultiThreadedEnabled(bool enabled) { m_useMultiThreaded = enabled; }

    //! マルチスレッド使用フラグ取得
    bool isMultiThreadedEnabled() const { return m_useMultiThreaded; }

    //! デバック描画
    //void debugImgui();

    //! 登録数取得
    size_t getComponentCount() const { return m_components.size(); }

    //! マルチスレッド計測情報（デバッグ用）
    struct ThreadTimingInfo
    {
        std::string name;
        float startMs = 0.0f;
        float durationMs = 0.0f;
        std::thread::id threadId{};
    };

private:

    RenderManager() = default;
    ~RenderManager() = default;
    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;

    std::vector<IRenderComponent*> m_components;
    std::mutex m_mutex;

    //! 計測情報
    std::vector<ThreadTimingInfo> m_timings;
    float m_totalMs = 0.0f;
    float m_singleEstimateMs = 0.0f;
    std::mutex m_timingMutex;

    bool m_useMultiThreaded = true;
};