#pragma once

#include <functional>

struct CrashReport
{
    std::filesystem::path dumpPath;
    std::wstring description;
    DWORD exceptionCode = 0;
    void* exceptionAddress = nullptr;
};

class CrashReporter
{
public:
    using ExternalCrashSink = std::function<void(const CrashReport&)>;

    static CrashReporter& Instance()
    {
        static CrashReporter instance;
        return instance;
    }

    void initialize();
    void shutdown();

    void setExternalSink(ExternalCrashSink sink);
    void setDumpDirectory(const std::filesystem::path& dumpDirectory);

    static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers);
    static void terminateHandler();

private:
    CrashReporter() = default;
    ~CrashReporter() = default;

    CrashReporter(const CrashReporter&) = delete;
    CrashReporter(CrashReporter&&) = delete;
    CrashReporter& operator=(const CrashReporter&) = delete;
    CrashReporter& operator=(CrashReporter&&) = delete;

    LONG handleUnhandledException(EXCEPTION_POINTERS* exceptionPointers);
    void handleTerminate();
    bool writeDump(EXCEPTION_POINTERS* exceptionPointers, const std::wstring& description);
    std::filesystem::path resolveDumpDirectory() const;
    std::filesystem::path buildDumpPath() const;

    std::mutex m_mutex;
    ExternalCrashSink m_externalSink;
    std::filesystem::path m_dumpDirectory;
    LPTOP_LEVEL_EXCEPTION_FILTER m_previousUnhandledExceptionFilter = nullptr;
    std::terminate_handler m_previousTerminateHandler = nullptr;
    bool m_initialized = false;
};
