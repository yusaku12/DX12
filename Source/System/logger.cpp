#include "pch.h"

namespace
{
    constexpr const char* kLogFileName = "DirectX12.log";
    constexpr const char* kLogDirectoryName = "DirectX12";
    constexpr const char* kLogSubDirectoryName = "Logs";
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

    m_logDirectory = getLogBaseDirectory();
    std::error_code ec;
    std::filesystem::create_directories(m_logDirectory, ec);
    openLogFile();
    m_initialized = true;
}

void Logger::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized)
    {
        return;
    }

    if (m_logFile.is_open())
    {
        m_logFile.flush();
        m_logFile.close();
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
    std::string fileLine;
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
        fileLine = std::format("[{}][{}] {}", makeTimestampString(), levelLabel, message);

        std::ostream* out = (level == LogLevel::INFO) ? static_cast<std::ostream*>(&std::cout) : static_cast<std::ostream*>(&std::cerr);
        (*out) << consoleLine << std::endl;

        if (m_logFile.is_open())
        {
            rotateLogFilesIfNeeded();
            m_logFile << fileLine << std::endl;
            m_logFile.flush();
            m_currentLogSize += fileLine.size() + 1;
        }

        m_imguiLogs.push_back({ consoleLine, color });
        if (m_imguiLogs.size() > 2048)
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

void Logger::openLogFile()
{
    std::error_code ec;
    m_logDirectory = getLogBaseDirectory();
    std::filesystem::create_directories(m_logDirectory, ec);

    m_currentLogFile = m_logDirectory / kLogFileName;
    if (std::filesystem::exists(m_currentLogFile, ec))
    {
        m_currentLogSize = std::filesystem::file_size(m_currentLogFile, ec);
    }
    else
    {
        m_currentLogSize = 0;
    }

    m_logFile.open(m_currentLogFile, std::ios::out | std::ios::app);
}

void Logger::rotateLogFilesIfNeeded()
{
    if (!m_logFile.is_open())
    {
        return;
    }

    if (m_currentLogSize < 2 * 1024 * 1024)
    {
        return;
    }

    m_logFile.flush();
    m_logFile.close();

    std::error_code ec;
    for (int index = 4; index >= 1; --index)
    {
        std::filesystem::path source = (index == 1) ? m_currentLogFile : m_logDirectory / std::format("DirectX12.{}.log", index - 1);
        std::filesystem::path destination = m_logDirectory / std::format("DirectX12.{}.log", index);

        if (std::filesystem::exists(destination, ec))
        {
            std::filesystem::remove(destination, ec);
        }

        if (std::filesystem::exists(source, ec))
        {
            std::filesystem::rename(source, destination, ec);
        }
    }

    m_currentLogSize = 0;
    m_logFile.open(m_currentLogFile, std::ios::out | std::ios::trunc);
}

std::string Logger::makeTimestampString()
{
    SYSTEMTIME time{};
    GetLocalTime(&time);
    return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond);
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
