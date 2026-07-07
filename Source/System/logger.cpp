#include "pch.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace
{
    constexpr const char* kLogFileName = "DirectX12.log";
    constexpr const char* kLogDirectoryName = "DirectX12";
    constexpr const char* kLogSubDirectoryName = "Logs";
    constexpr size_t kMaxLogEntries = 2048;
    constexpr size_t kMaxLogFileSize = 2 * 1024 * 1024;
    constexpr size_t kMaxLogFileCount = 5;
}

void Logger::setExternalSink(std::function<void(LogLevel, const std::string&)>&& sink)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_externalSink = std::move(sink);
}

void Logger::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
    {
        return;
    }

    const std::filesystem::path logDirectory = getLogBaseDirectory();
    std::error_code ec;
    std::filesystem::create_directories(logDirectory, ec);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (logDirectory / kLogFileName).string(),
        kMaxLogFileSize,
        kMaxLogFileCount));
#if defined(_WIN32)
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

    m_logger = std::make_shared<spdlog::logger>("dx12", sinks.begin(), sinks.end());
    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$] %v");
    m_logger->set_level(spdlog::level::trace);
    m_logger->flush_on(spdlog::level::warn);

    m_initialized = true;
}

void Logger::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized)
    {
        return;
    }

    if (m_logger)
    {
        m_logger->flush();
        m_logger.reset();
    }

    m_initialized = false;
}

void Logger::renderLog()
{
    ImGui::Begin("Console");
    renderLogContents();
    ImGui::End();
}

void Logger::renderLogContents()
{
    std::vector<ImGuiLogEntry> logs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        logs = m_imguiLogs;
    }

    ImGui::BeginChild("LogArea", ImVec2(0, 0), true);
    for (const auto& entry : logs)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, entry.color);
        ImGui::TextWrapped("%s", entry.message.c_str());
        ImGui::PopStyleColor();
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void Logger::log(LogLevel level, const std::string& message)
{
    std::function<void(LogLevel, const std::string&)> externalSink;
    std::string consoleLine;
    ImVec4 color = levelColor(level);

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_initialized)
        {
            lock.unlock();
            initialize();
            lock.lock();
        }

        const char* levelLabel = levelName(level);
        consoleLine = std::format("[{}] {}", levelLabel, message);

        if (m_logger)
        {
            m_logger->log(toSpdLevel(level), message);
        }

        m_imguiLogs.push_back({ consoleLine, color });
        if (m_imguiLogs.size() > kMaxLogEntries)
        {
            m_imguiLogs.erase(m_imguiLogs.begin());
        }

        externalSink = m_externalSink;
    }

    if (externalSink)
    {
        externalSink(level, message);
    }
}

spdlog::level::level_enum Logger::toSpdLevel(LogLevel level)
{
    switch (level)
    {
    case LogLevel::INFO:
        return spdlog::level::info;
    case LogLevel::WARN:
        return spdlog::level::warn;
    case LogLevel::ERROR:
        return spdlog::level::err;
    default:
        return spdlog::level::info;
    }
}

const char* Logger::levelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

ImVec4 Logger::levelColor(LogLevel level)
{
    switch (level)
    {
    case LogLevel::INFO:
        return ImVec4(0.45f, 0.95f, 1.0f, 1.0f);
    case LogLevel::WARN:
        return ImVec4(1.0f, 0.85f, 0.25f, 1.0f);
    case LogLevel::ERROR:
        return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    default:
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

std::filesystem::path Logger::getLogBaseDirectory()
{
    wchar_t localAppData[MAX_PATH]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    if (length > 0 && length < std::size(localAppData))
    {
        return std::filesystem::path(localAppData) / kLogDirectoryName / kLogSubDirectoryName;
    }

    return std::filesystem::current_path() / kLogDirectoryName / kLogSubDirectoryName;
}
