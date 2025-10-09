#pragma once

//! ���O���x��
enum class LogLevel
{
    INFO,
    WARN,
    ASSERT
};

//=====================================================
// Logger �N���X
//=====================================================
class Logger
{
public:

    //! �V���O���g���擾
    static Logger& getInstance()
    {
        static Logger instance;
        return instance;
    }

    //! �����w�胍�O�o�́i�ψ����Ή��j
    template<typename... Args>
    void logCall(LogLevel level, const std::string& format, Args&&... args)
    {
        log(level, stringFormat(format, std::forward<Args>(args)...));
    }

    //! ���O�\��
    void renderLog();

private:

    Logger() = default;
    ~Logger() = default;

    // �R�s�[/���[�u�֎~
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    //! ���ۂ̃��O�o�͏���
    void log(LogLevel level, const std::string& message);

    //! ImGui �ł̃��O�ێ��\����
    struct ImGuiLogEntry
    {
        std::string message;  //!< �{��
        ImVec4 color;         //!< �\���F
    };

    std::mutex m_mutex;                     //!< �X���b�h���S�p
    std::vector<ImGuiLogEntry> m_imguiLogs; //!< ImGui �p���O
};