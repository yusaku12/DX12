#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

struct CrashReport
{
    uint32_t schemaVersion = 1;
    std::wstring crashId;
    std::filesystem::path dumpPath;
    std::filesystem::path metadataPath;
    std::wstring description;
    DWORD exceptionCode = 0;
    void* exceptionAddress = nullptr;
    DWORD processId = 0;
    DWORD threadId = 0;
    uint64_t timestampUtcMilliseconds = 0;
    bool recoveredFromQueue = false;
};

class CrashReporter
{
public:
    using ExternalCrashSink = std::function<void(const CrashReport&)>;

    struct UploadSettings
    {
        std::wstring endpointUrl;
        std::wstring bearerToken;
    };

    static CrashReporter& Instance()
    {
        static CrashReporter instance;
        return instance;
    }

    void initialize();
    void shutdown();

    void setExternalSink(ExternalCrashSink sink);
    void setDumpDirectory(const std::filesystem::path& dumpDirectory);
    void setQueueDirectory(const std::filesystem::path& queueDirectory);
    void setUploadEndpoint(const std::wstring& endpointUrl, const std::wstring& bearerToken = L"");
    void clearUploadEndpoint();
    void dispatchPendingReports();

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
    bool writeMetadataFile(const CrashReport& report) const;
    bool enqueueReport(const CrashReport& report, const std::filesystem::path& queueDirectory) const;
    bool sendReportToEndpoint(const CrashReport& report, const UploadSettings& settings) const;
    static std::string buildUploadPayload(const CrashReport& report);
    void dispatchPendingReportsLocked();
    std::filesystem::path resolveDumpDirectory() const;
    std::filesystem::path resolveQueueDirectory() const;
    std::wstring buildCrashId() const;
    std::filesystem::path buildDumpPath() const;
    std::filesystem::path buildDumpPath(const std::wstring& crashId) const;
    std::filesystem::path buildMetadataPath(const std::wstring& crashId) const;
    bool parseMetadataFile(const std::filesystem::path& filePath, CrashReport& outReport) const;

    std::mutex m_mutex;
    ExternalCrashSink m_externalSink;
    UploadSettings m_uploadSettings;
    std::filesystem::path m_dumpDirectory;
    std::filesystem::path m_queueDirectory;
    LPTOP_LEVEL_EXCEPTION_FILTER m_previousUnhandledExceptionFilter = nullptr;
    std::terminate_handler m_previousTerminateHandler = nullptr;
    bool m_initialized = false;
};
