#pragma once

#include "LoadTexture.h"

//=====================================================
// 読み込んだテクスチャをキャッシュするシングルトン
//=====================================================
class TextureManager
{
public:
    enum class StreamPriority : uint8_t
    {
        Critical = 0,
        High,
        Normal,
        Low,
    };

    struct StreamProgress
    {
        uint64_t requestId = 0;
        std::wstring filePath;
        float normalized = 0.0f;
        bool done = false;
        bool success = false;
        bool cancelled = false;
        std::string stage;
    };

    using StreamProgressCallback = std::function<void(const StreamProgress&)>;

    struct StreamRequestResult
    {
        bool accepted = false;
        uint64_t requestId = 0;

        explicit operator bool() const { return accepted; }
    };


    //! シングルトンインスタンス取得
    static TextureManager& Instance()
    {
        static TextureManager instance;
        return instance;
    }

    //! テクスチャをロード（キャッシュあり）
    LoadTexture* load(const std::wstring& filePath);

    //! キャッシュ済みテクスチャ取得（未ロード時は nullptr）
    LoadTexture* findCached(const std::wstring& filePath);

    //! ストリーミング読み込み要求（プレースホルダを即時返却）
    StreamRequestResult requestStreaming(const std::wstring& filePath,
        StreamPriority priority = StreamPriority::Normal,
        StreamProgressCallback progressCallback = {});

    //! ストリーミング要求をキャンセル
    bool cancelStreaming(uint64_t requestId);

    //! 全ストリーミング要求をキャンセル
    void cancelAllStreaming();

    //! ストリーミング更新（毎フレーム呼び出し）
    void updateStreaming();

    //! ストリーミング稼働状態
    bool isStreamingBusy() const;

    //! ストリーミング待機件数
    size_t streamingPendingCount() const;

    //! 全テクスチャ解放
    void clear();

    //! フォールバック白テクスチャ取得
    LoadTexture* getFallbackTexture();

private:
    struct StreamRequest
    {
        uint64_t id = 0;
        std::wstring filePath;
        StreamPriority priority = StreamPriority::Normal;
        uint64_t sequence = 0;
        StreamProgressCallback progressCallback;
    };

    struct StreamResult
    {
        StreamRequest request;
        bool ok = false;
        std::string message;
        LoadTexture::DecodedData decoded;
    };

    struct StreamWorker
    {
        StreamRequest request;
        std::future<StreamResult> future;
    };

    void dispatchStreamingRequests();
    void pumpStreamingWorkers();
    void applyStreamingResults();
    bool isStreamingCancelled(uint64_t requestId) const;
    void reportStreaming(const StreamRequest& request,
        float normalized,
        const char* stage,
        bool done = false,
        bool success = false,
        bool cancelled = false);
    StreamRequestResult requestStreamingInternal(const std::wstring& filePath,
        StreamPriority priority,
        StreamProgressCallback progressCallback);
    static StreamResult decodeStreamingRequest(StreamRequest request);

private:

    TextureManager() = default;
    ~TextureManager() = default;

    //! テクスチャキャッシュ
    std::unordered_map<std::wstring, std::unique_ptr<LoadTexture>> m_textureCache;
    std::unique_ptr<LoadTexture> m_fallbackTexture;

    uint64_t m_nextStreamRequestId = 1;
    uint64_t m_nextStreamSequence = 1;
    std::deque<StreamRequest> m_streamPending;
    std::vector<StreamWorker> m_streamWorkers;
    std::deque<StreamResult> m_streamCompleted;
    std::unordered_set<uint64_t> m_streamCancelledIds;

    static constexpr size_t kMaxStreamWorkers = 2;
};