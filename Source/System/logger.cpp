#include "pch.h"

//! コンソール出力文字色制御コード
static constexpr const char* ConsoleColorReset = "\x1b[0m";
static constexpr const char* ConsoleColorBlack = "\x1b[30m";
static constexpr const char* ConsoleColorRed = "\x1b[31m";
static constexpr const char* ConsoleColorGreen = "\x1b[32m";
static constexpr const char* ConsoleColorYellow = "\x1b[33m";
static constexpr const char* ConsoleColorBlue = "\x1b[34m";
static constexpr const char* ConsoleColorMagenta = "\x1b[35m";
static constexpr const char* ConsoleColorCyan = "\x1b[36m";
static constexpr const char* ConsoleColorWhite = "\x1b[37m";

void Logger::renderLog()
{
    ImGui::Begin("Log");
    const auto& logs = m_imguiLogs;
    ImGui::BeginChild("LogArea", ImVec2(0, 0), true);
    for (const auto& entry : logs)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, entry.color);
        ImGui::TextWrapped("%s", entry.message.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f); //!< 自動スクロール
    }
    ImGui::EndChild();
    ImGui::End();
}

void Logger::log(LogLevel level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->m_mutex);

    const char* prefix = "";
    std::ostream* out = &std::cout;

    switch (level)
    {
    case LogLevel::INFO:  prefix = "[INFO] "; out = &std::cout; break;
    case LogLevel::WARN:  prefix = "[ASSERT] "; out = &std::cerr; break;
    case LogLevel::ERROR: prefix = "[ERROR] "; out = &std::cerr; break;
    }

    (*out) << prefix << message << std::endl;

    // ImGui 用にも保存
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    if (level == LogLevel::INFO) color = ImVec4(0.f, 1.f, 1.f, 1.f);
    if (level == LogLevel::WARN) color = ImVec4(1.f, 1.f, 0.f, 1.f);
    if (level == LogLevel::ERROR) color = ImVec4(1.f, 0.f, 0.f, 1.f);

    m_imguiLogs.push_back({ prefix + message, color });
}