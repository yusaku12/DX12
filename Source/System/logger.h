#pragma once

//! ログレベル
enum class LogLevel
{
    INFO,
    WARN,
    ASSERT
};

//=====================================================
// Logger クラス
//=====================================================
class Logger
{
public:

    //! シングルトン取得
    static Logger& getInstance()
    {
        static Logger instance;
        return instance;
    }

    //! 書式指定ログ出力（可変引数対応）
    template<typename... Args>
    void logCall(LogLevel level, const std::string& format, Args&&... args)
    {
        log(level, stringFormat(format, std::forward<Args>(args)...));
    }

    //! ログ表示
    void renderLog();

private:

    Logger() = default;
    ~Logger() = default;

    // コピー/ムーブ禁止
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    //! 実際のログ出力処理
    void log(LogLevel level, const std::string& message);

    //! ImGui でのログ保持構造体
    struct ImGuiLogEntry
    {
        std::string message;  //!< 本文
        ImVec4 color;         //!< 表示色
    };

    std::mutex m_mutex;                     //!< スレッド安全用
    std::vector<ImGuiLogEntry> m_imguiLogs; //!< ImGui 用ログ
};