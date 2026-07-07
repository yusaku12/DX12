#pragma once

class IRenderComponent;

#include <functional>
#pragma warning(push)
#pragma warning(disable:4324)
#include <taskflow/taskflow.hpp>
#pragma warning(pop)
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

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

    //! シャドウ深度描画（シングルスレッド・カスケード境界カリング対応）
    void renderShadowCasters(const DirectX::BoundingOrientedBox& cascadeOBB);

    //! マルチスレッド使用フラグ設定
    void setMultiThreadedEnabled(bool enabled) { m_useMultiThreaded = enabled; }

    //! マルチスレッド使用フラグ取得
    bool isMultiThreadedEnabled() const { return m_useMultiThreaded; }

    //! デバッグImGui描画
    void debugImgui();

    //! デバッグ中身表示
    void renderDebugContents();

    //! 登録数取得
    size_t getComponentCount() const { return m_components.size(); }

    //! カリング設定
    void setFrustumCullingEnabled(bool enabled) { m_enableFrustumCulling = enabled; }
    bool isFrustumCullingEnabled() const { return m_enableFrustumCulling; }

    //! 自動LOD設定
    void setAutoLodEnabled(bool enabled) { m_enableAutoLod = enabled; }
    bool isAutoLodEnabled() const { return m_enableAutoLod; }

    //! 自動HLOD設定（親にRenderComponentがあり、子孫にもRenderComponentがある階層を対象）
    void setAutoHlodEnabled(bool enabled) { m_enableAutoHlod = enabled; }
    bool isAutoHlodEnabled() const { return m_enableAutoHlod; }

    //! GPU Occlusion Query 設定
    void setGpuOcclusionEnabled(bool enabled) { m_enableGpuOcclusion = enabled; }
    bool isGpuOcclusionEnabled() const { return m_enableGpuOcclusion; }

    //! HLOD切替距離
    void setHlodSwitchDistance(float distance) { m_hlodSwitchDistance = std::max(distance, 0.0f); }
    float getHlodSwitchDistance() const { return m_hlodSwitchDistance; }

    //! フレーム開始通知（Occlusionフィードバック更新の準備）
    void notifyFrameStart() { m_occlusionFramePrepared = false; }

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

    //! 現在カメラ視錐台で可視コンポーネントを抽出
    std::vector<IRenderComponent*> collectVisibleComponents(const std::vector<IRenderComponent*>& comps);

    //! 自動HLODフィルタを適用して描画対象を絞る
    std::vector<IRenderComponent*> applyAutoHlod(const std::vector<IRenderComponent*>& comps);

    //! 自動LODを各コンポーネントへ反映
    void applyAutoLod(const std::vector<IRenderComponent*>& comps);

    //! フレーム開始時の GPU Occlusion Query フィードバック更新
    void beginOcclusionFrame();

    //! Occlusion 判定で描画をスキップしてよいか
    bool isOcclusionVisible(IRenderComponent* comp) const;

    //! Occlusion Query の発行可否
    bool shouldIssueOcclusionQuery(IRenderComponent* comp) const;

    //! Occlusion Query を発行して描画関数を実行
    void executeWithOcclusionQuery(ID3D12GraphicsCommandList* cmd, IRenderComponent* comp, const std::function<void()>& drawFn);

    //! Occlusion Query リソースを必要数に拡張
    void ensureOcclusionCapacity(UINT requiredCount);

    //! Query インデックスを確保
    bool allocateOcclusionQueryIndex(IRenderComponent* comp, UINT& outIndex);

    struct OcclusionQueryState
    {
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap;
        Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
        UINT capacity = 0;
        UINT used = 0;
        std::vector<IRenderComponent*> indexToComponent;
    };

    std::vector<IRenderComponent*> m_components;
    tf::Executor m_taskExecutor;
    std::mutex m_mutex;
    mutable std::mutex m_occlusionMutex;

    //! 計測情報
    std::vector<ThreadTimingInfo> m_timings;
    float m_totalMs = 0.0f;
    float m_singleEstimateMs = 0.0f;
    std::mutex m_timingMutex;

    bool m_useMultiThreaded = true;
    bool m_enableFrustumCulling = true;
    bool m_enableAutoLod = true;
    bool m_enableAutoHlod = true;
    bool m_enableGpuOcclusion = true;
    float m_hlodSwitchDistance = 60.0f;
    bool m_occlusionFramePrepared = false;

    OcclusionQueryState m_occlusionState;
    std::unordered_map<IRenderComponent*, bool> m_occlusionVisibleByComponent;

    size_t m_lastSubmittedCount = 0;
    size_t m_lastVisibleCount = 0;
    size_t m_lastFrustumCulledCount = 0;
    size_t m_lastOcclusionCulledCount = 0;
    size_t m_lastOcclusionQueryCount = 0;
    size_t m_lastHlodMergedCount = 0;
    size_t m_lastLodAdjustedCount = 0;
};