#include "pch.h"

LoadTexture* TextureManager::load(const std::wstring& filePath)
{
    // キャッシュに存在すれば再利用
    auto it = m_textureCache.find(filePath);
    if (it != m_textureCache.end())
    {
        return it->second.get();
    }

    // 存在しない場合 → 新規ロード
    auto newTex = DXMem::makeUnique<LoadTexture>(filePath.c_str());

    // 失敗した場合は白色テクスチャを返す
    if (!newTex->isValid())
    {
        uint8_t whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        newTex = DXMem::makeUnique<LoadTexture>(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, whitePixel, sizeof(whitePixel));
    }

    LoadTexture* texPtr = newTex.get();
    m_textureCache[filePath] = std::move(newTex);

    std::wstring filename = L"Texture Loaded:" + filePath + L"\n";
    LOG_INFO(wstringToString(filename));
    return texPtr;
}

LoadTexture* TextureManager::findCached(const std::wstring& filePath)
{
    auto it = m_textureCache.find(filePath);
    if (it == m_textureCache.end())
    {
        return nullptr;
    }

    return it->second.get();
}

TextureManager::StreamRequestResult TextureManager::requestStreaming(const std::wstring& filePath,
    StreamPriority priority,
    StreamProgressCallback progressCallback)
{
    return requestStreamingInternal(filePath, priority, std::move(progressCallback));
}

bool TextureManager::cancelStreaming(uint64_t requestId)
{
    if (requestId == 0)
    {
        return false;
    }

    auto pendingIt = std::find_if(m_streamPending.begin(), m_streamPending.end(),
        [requestId](const StreamRequest& request)
        {
            return request.id == requestId;
        });

    if (pendingIt != m_streamPending.end())
    {
        reportStreaming(*pendingIt, 1.0f, "Cancelled", true, false, true);
        m_streamPending.erase(pendingIt);
        m_streamCancelledIds.insert(requestId);
        return true;
    }

    for (const StreamWorker& worker : m_streamWorkers)
    {
        if (worker.request.id == requestId)
        {
            m_streamCancelledIds.insert(requestId);
            reportStreaming(worker.request, 1.0f, "Cancel requested", false, false, true);
            return true;
        }
    }

    return false;
}

void TextureManager::cancelAllStreaming()
{
    for (const StreamRequest& request : m_streamPending)
    {
        m_streamCancelledIds.insert(request.id);
        reportStreaming(request, 1.0f, "Cancelled", true, false, true);
    }
    m_streamPending.clear();

    for (const StreamWorker& worker : m_streamWorkers)
    {
        m_streamCancelledIds.insert(worker.request.id);
    }

    for (const StreamResult& result : m_streamCompleted)
    {
        m_streamCancelledIds.insert(result.request.id);
    }
}

void TextureManager::updateStreaming()
{
    dispatchStreamingRequests();
    pumpStreamingWorkers();
    applyStreamingResults();
}

bool TextureManager::isStreamingBusy() const
{
    return !m_streamPending.empty() || !m_streamWorkers.empty() || !m_streamCompleted.empty();
}

size_t TextureManager::streamingPendingCount() const
{
    return m_streamPending.size() + m_streamWorkers.size() + m_streamCompleted.size();
}

void TextureManager::clear()
{
    cancelAllStreaming();
    m_streamWorkers.clear();
    m_streamCompleted.clear();
    m_streamCancelledIds.clear();
    m_textureCache.clear();
    m_fallbackTexture.reset();
    LOG_INFO("[TextureManager] Cleared all textures");
}

LoadTexture* TextureManager::getFallbackTexture()
{
    if (!m_fallbackTexture)
    {
        uint8_t whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        m_fallbackTexture = DXMem::makeUnique<LoadTexture>(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, whitePixel, sizeof(whitePixel));
    }

    return m_fallbackTexture.get();
}

void TextureManager::dispatchStreamingRequests()
{
    while (!m_streamPending.empty() && m_streamWorkers.size() < kMaxStreamWorkers)
    {
        StreamRequest request = std::move(m_streamPending.front());
        m_streamPending.pop_front();

        if (isStreamingCancelled(request.id))
        {
            reportStreaming(request, 1.0f, "Cancelled", true, false, true);
            continue;
        }

        reportStreaming(request, 0.2f, "Decoding");

        StreamWorker worker;
        worker.request = request;
        worker.future = std::async(std::launch::async, [request]()
            {
                return decodeStreamingRequest(request);
            });
        m_streamWorkers.push_back(std::move(worker));
    }
}

void TextureManager::pumpStreamingWorkers()
{
    auto it = m_streamWorkers.begin();
    while (it != m_streamWorkers.end())
    {
        if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            StreamResult result = it->future.get();
            if (isStreamingCancelled(result.request.id))
            {
                reportStreaming(result.request, 1.0f, "Cancelled", true, false, true);
            }
            else
            {
                reportStreaming(result.request, 0.7f, result.ok ? "Decoded" : "Decode failed");
                m_streamCompleted.push_back(std::move(result));
            }

            it = m_streamWorkers.erase(it);
            continue;
        }

        ++it;
    }
}

void TextureManager::applyStreamingResults()
{
    constexpr size_t kMaxApplyPerFrame = 2;
    size_t applied = 0;

    while (!m_streamCompleted.empty() && applied < kMaxApplyPerFrame)
    {
        StreamResult result = std::move(m_streamCompleted.front());
        m_streamCompleted.pop_front();

        if (isStreamingCancelled(result.request.id))
        {
            reportStreaming(result.request, 1.0f, "Cancelled", true, false, true);
            ++applied;
            continue;
        }

        if (!result.ok)
        {
            reportStreaming(result.request, 1.0f, result.message.c_str(), true, false, false);
            ++applied;
            continue;
        }

        auto it = m_textureCache.find(result.request.filePath);
        if (it == m_textureCache.end() || !it->second)
        {
            uint8_t whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
            it = m_textureCache.emplace(result.request.filePath,
                DXMem::makeUnique<LoadTexture>(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, whitePixel, sizeof(whitePixel))).first;
        }

        const bool replaced = it->second->replaceFromDecoded(std::move(result.decoded));
        if (!replaced)
        {
            reportStreaming(result.request, 1.0f, "GPU upload failed", true, false, false);
            ++applied;
            continue;
        }

        reportStreaming(result.request, 1.0f, "Ready", true, true, false);
        m_streamCancelledIds.erase(result.request.id);
        ++applied;
    }
}

bool TextureManager::isStreamingCancelled(uint64_t requestId) const
{
    return m_streamCancelledIds.find(requestId) != m_streamCancelledIds.end();
}

void TextureManager::reportStreaming(const StreamRequest& request,
    float normalized,
    const char* stage,
    bool done,
    bool success,
    bool cancelled)
{
    if (request.progressCallback)
    {
        StreamProgress progress;
        progress.requestId = request.id;
        progress.filePath = request.filePath;
        progress.normalized = std::clamp(normalized, 0.0f, 1.0f);
        progress.done = done;
        progress.success = success;
        progress.cancelled = cancelled;
        progress.stage = stage ? stage : "";
        request.progressCallback(progress);
    }
}

TextureManager::StreamRequestResult TextureManager::requestStreamingInternal(const std::wstring& filePath,
    StreamPriority priority,
    StreamProgressCallback progressCallback)
{
    StreamRequestResult out{};
    if (filePath.empty())
    {
        return out;
    }

    auto found = m_textureCache.find(filePath);
    if (found == m_textureCache.end())
    {
        uint8_t whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        m_textureCache.emplace(filePath,
            DXMem::makeUnique<LoadTexture>(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, whitePixel, sizeof(whitePixel)));
    }

    StreamRequest request;
    request.id = m_nextStreamRequestId++;
    request.filePath = filePath;
    request.priority = priority;
    request.sequence = m_nextStreamSequence++;
    request.progressCallback = std::move(progressCallback);

    m_streamPending.push_back(request);
    std::stable_sort(m_streamPending.begin(), m_streamPending.end(),
        [](const StreamRequest& lhs, const StreamRequest& rhs)
        {
            if (lhs.priority != rhs.priority)
            {
                return static_cast<int>(lhs.priority) < static_cast<int>(rhs.priority);
            }
            return lhs.sequence < rhs.sequence;
        });

    reportStreaming(request, 0.0f, "Queued");

    out.accepted = true;
    out.requestId = request.id;
    return out;
}

TextureManager::StreamResult TextureManager::decodeStreamingRequest(StreamRequest request)
{
    StreamResult result;
    result.request = std::move(request);

    std::string errorMessage;
    result.ok = LoadTexture::decodeFromFile(result.request.filePath, result.decoded, &errorMessage);
    if (!result.ok)
    {
        result.message = errorMessage.empty() ? "Decode failed" : errorMessage;
    }
    else
    {
        result.message = "Decoded";
    }

    return result;
}