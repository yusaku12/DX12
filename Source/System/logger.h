#pragma once

//! ログレベル
enum class LogLevel : int
{
    INFO,
    WARN,
    ERROR
};

//=====================================================
// ログ出力を管理するシングルトン
//=====================================================
class Logger
{
public:

    //! シングルトン取得
    static Logger& Instance()
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

    //! 外部サービス向けのフックを設定
    void setExternalSink(std::function<void(LogLevel, const std::string&)>&& sink);

    //! ロガー初期化
    void initialize();

    //! ロガー終了
    void shutdown();

    //! ログ表示
    void renderLog();

    //! ログ中身表示
    void renderLogContents();

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

    //! ファイルを開く
    void openLogFile();

    //! ローテーション処理
    void rotateLogFilesIfNeeded();

    //! ログ保存先ベースディレクトリ
    static std::filesystem::path getLogBaseDirectory();

    //! 現在時刻をファイル名向けに文字列化
    static std::string makeTimestampString();

    //! ログレベル文字列
    static const char* levelName(LogLevel level);

    //! ログレベル表示色
    static ImVec4 levelColor(LogLevel level);

    //! ImGui でのログ保持構造体
    struct ImGuiLogEntry
    {
        std::string message;  //!< 本文
        ImVec4 color;         //!< 表示色
    };

    std::mutex m_mutex;                     //!< スレッド安全用
    std::vector<ImGuiLogEntry> m_imguiLogs; //!< ImGui 用ログ
    std::function<void(LogLevel, const std::string&)> m_externalSink; //!< 外部連携フック
    std::filesystem::path m_logDirectory;   //!< ログ保存先ディレクトリ
    std::filesystem::path m_currentLogFile; //!< 現在のログファイル
    std::ofstream m_logFile;                //!< ファイル出力
    std::uintmax_t m_currentLogSize = 0;    //!< 現在サイズ
    bool m_initialized = false;             //!< 初期化済み
};