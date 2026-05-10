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

    //! 終了処理
    void shutdown();

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

    //! デバッグImGui描画
    void debugImgui();

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

    enum class RenderPassKind
    {
        Default,
        GBuffer,
        Forward
    };

    RenderManager() = default;
    ~RenderManager() = default;
    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;

    //! 登録コンポーネントのコピー取得（スレッドセーフ）
    std::vector<IRenderComponent*> copyComponents();

    //! シングルスレッド描画用の計測情報記録
    void recordSingleThreadTiming(const std::string& name, float totalMs);

    //! マルチスレッド描画用の計測情報クリア
    void clearTimings();

    //! マルチスレッド描画用の計測情報追加
    void addTiming(const std::string& name, float startMs, float durationMs, std::thread::id threadId);

    //! 描画完了後の計測情報処理
    void finalizeTimings(float totalMs);

    //! コマンドリストの共通セットアップ
    void setupCommandList(RenderPassKind kind, ID3D12GraphicsCommandList* cmd);

    //! 描画コマンド記録の共通処理
    void executeRender(RenderPassKind kind, IRenderComponent* comp, ID3D12GraphicsCommandList* cmd);

    //! シングルスレッド描画の内部実装
    void renderSingleThreadedInternal(RenderPassKind kind);

    //! マルチスレッド描画の内部実装
    void renderMultiThreadedInternal(RenderPassKind kind);

    std::vector<IRenderComponent*> m_components;
    std::mutex m_mutex;

    //! 計測情報
    std::vector<ThreadTimingInfo> m_timings;
    float m_totalMs = 0.0f;
    float m_singleEstimateMs = 0.0f;
    std::mutex m_timingMutex;

    bool m_useMultiThreaded = true;
};