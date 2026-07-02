#include "pch.h"
#include <dbghelp.h>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>

#pragma comment(lib, "dbghelp.lib")

namespace
{
    constexpr const wchar_t* kCrashDirectoryName = L"DirectX12";
    constexpr const wchar_t* kCrashSubDirectoryName = L"CrashDumps";
    constexpr const wchar_t* kCrashQueueSubDirectoryName = L"CrashQueue";
    constexpr uint32_t kCrashMetadataSchemaVersion = 1;

    std::string trimAscii(std::string_view value)
    {
        size_t begin = 0;
        size_t end = value.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return std::string(value.substr(begin, end - begin));
    }

    uint64_t currentUtcMilliseconds()
    {
        const auto now = std::chrono::system_clock::now();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }

    std::string pointerToString(const void* ptr)
    {
        return std::format("0x{:X}", reinterpret_cast<uintptr_t>(ptr));
    }

    bool tryParseUInt64(std::string_view text, uint64_t& outValue)
    {
        const std::string input = trimAscii(text);
        if (input.empty())
        {
            return false;
        }

        char* end = nullptr;
        errno = 0;
        const unsigned long long parsed = std::strtoull(input.c_str(), &end, 10);
        if (errno != 0 || !end || *end != '\0')
        {
            return false;
        }

        outValue = static_cast<uint64_t>(parsed);
        return true;
    }

    bool tryParseUInt32(std::string_view text, uint32_t& outValue)
    {
        uint64_t parsed = 0;
        if (!tryParseUInt64(text, parsed) || parsed > std::numeric_limits<uint32_t>::max())
        {
            return false;
        }

        outValue = static_cast<uint32_t>(parsed);
        return true;
    }

    bool tryParsePointer(std::string_view text, void*& outPointer)
    {
        std::string input = trimAscii(text);
        if (input.empty())
        {
            outPointer = nullptr;
            return true;
        }

        if (input.size() > 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X'))
        {
            input = input.substr(2);
        }

        char* end = nullptr;
        errno = 0;
        const unsigned long long parsed = std::strtoull(input.c_str(), &end, 16);
        if (errno != 0 || !end || *end != '\0')
        {
            return false;
        }

        outPointer = reinterpret_cast<void*>(static_cast<uintptr_t>(parsed));
        return true;
    }
}

void CrashReporter::initialize()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized)
        {
            return;
        }

        m_dumpDirectory = resolveDumpDirectory();
        m_queueDirectory = resolveQueueDirectory();

        std::error_code ec;
        std::filesystem::create_directories(m_dumpDirectory, ec);
        std::filesystem::create_directories(m_queueDirectory, ec);

        m_previousUnhandledExceptionFilter = SetUnhandledExceptionFilter(&CrashReporter::unhandledExceptionFilter);
        m_previousTerminateHandler = std::set_terminate(&CrashReporter::terminateHandler);
        m_initialized = true;
    }

    dispatchPendingReports();
}

void CrashReporter::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized)
    {
        return;
    }

    SetUnhandledExceptionFilter(m_previousUnhandledExceptionFilter);
    std::set_terminate(m_previousTerminateHandler);

    m_previousUnhandledExceptionFilter = nullptr;
    m_previousTerminateHandler = nullptr;
    m_initialized = false;
}

void CrashReporter::setExternalSink(ExternalCrashSink sink)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_externalSink = std::move(sink);
    }

    dispatchPendingReports();
}

void CrashReporter::setDumpDirectory(const std::filesystem::path& dumpDirectory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dumpDirectory = dumpDirectory;
}

void CrashReporter::setQueueDirectory(const std::filesystem::path& queueDirectory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queueDirectory = queueDirectory;
}

void CrashReporter::dispatchPendingReports()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    dispatchPendingReportsLocked();
}

LONG WINAPI CrashReporter::unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    return Instance().handleUnhandledException(exceptionPointers);
}

void CrashReporter::terminateHandler()
{
    Instance().handleTerminate();
}

LONG CrashReporter::handleUnhandledException(EXCEPTION_POINTERS* exceptionPointers)
{
    writeDump(exceptionPointers, L"Unhandled exception");
    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashReporter::handleTerminate()
{
    writeDump(nullptr, L"std::terminate");
    TerminateProcess(GetCurrentProcess(), 1);
}

bool CrashReporter::writeDump(EXCEPTION_POINTERS* exceptionPointers, const std::wstring& description)
{
    ExternalCrashSink externalSink;
    std::filesystem::path queueDirectory;
    CrashReport report;
    report.schemaVersion = kCrashMetadataSchemaVersion;
    report.description = description;
    report.processId = GetCurrentProcessId();
    report.threadId = GetCurrentThreadId();
    report.timestampUtcMilliseconds = currentUtcMilliseconds();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_dumpDirectory.empty())
        {
            m_dumpDirectory = resolveDumpDirectory();
        }
        if (m_queueDirectory.empty())
        {
            m_queueDirectory = resolveQueueDirectory();
        }

        std::error_code ec;
        std::filesystem::create_directories(m_dumpDirectory, ec);
        std::filesystem::create_directories(m_queueDirectory, ec);

        report.crashId = buildCrashId();
        report.dumpPath = buildDumpPath(report.crashId);
        report.metadataPath = buildMetadataPath(report.crashId);
        queueDirectory = m_queueDirectory;
        externalSink = m_externalSink;
    }

    report.exceptionCode = exceptionPointers && exceptionPointers->ExceptionRecord ? exceptionPointers->ExceptionRecord->ExceptionCode : 0;
    report.exceptionAddress = exceptionPointers && exceptionPointers->ExceptionRecord ? exceptionPointers->ExceptionRecord->ExceptionAddress : nullptr;

    HANDLE dumpFile = CreateFileW(
        report.dumpPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (dumpFile == INVALID_HANDLE_VALUE)
    {
        if (externalSink)
        {
            externalSink(report);
        }
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
    MINIDUMP_EXCEPTION_INFORMATION* exceptionInfoPtr = nullptr;
    if (exceptionPointers)
    {
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = exceptionPointers;
        exceptionInfo.ClientPointers = FALSE;
        exceptionInfoPtr = &exceptionInfo;
    }

    MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        dumpFile,
        dumpType,
        exceptionInfoPtr,
        nullptr,
        nullptr);

    CloseHandle(dumpFile);

    writeMetadataFile(report);
    enqueueReport(report, queueDirectory);

    if (externalSink)
    {
        externalSink(report);

        std::error_code ec;
        if (!queueDirectory.empty())
        {
            const std::filesystem::path queueFile = queueDirectory / (report.crashId + L".crashmeta");
            std::filesystem::remove(queueFile, ec);
        }
    }

    return true;
}

bool CrashReporter::writeMetadataFile(const CrashReport& report) const
{
    if (report.metadataPath.empty())
    {
        return false;
    }

    std::ofstream out(report.metadataPath, std::ios::trunc);
    if (!out)
    {
        LOG_WARN("[CrashReporter] Failed to write metadata file: %s", report.metadataPath.string().c_str());
        return false;
    }

    out << "schemaVersion=" << report.schemaVersion << "\n";
    out << "crashId=" << wstringToString(report.crashId) << "\n";
    out << "description=" << wstringToString(report.description) << "\n";
    out << "dumpPath=" << report.dumpPath.generic_string() << "\n";
    out << "exceptionCode=" << report.exceptionCode << "\n";
    out << "exceptionAddress=" << pointerToString(report.exceptionAddress) << "\n";
    out << "processId=" << report.processId << "\n";
    out << "threadId=" << report.threadId << "\n";
    out << "timestampUtcMs=" << report.timestampUtcMilliseconds << "\n";
    return out.good();
}

bool CrashReporter::enqueueReport(const CrashReport& report, const std::filesystem::path& queueDirectory) const
{
    if (queueDirectory.empty())
    {
        return false;
    }

    const std::filesystem::path queueFile = queueDirectory / (report.crashId + L".crashmeta");
    std::ofstream out(queueFile, std::ios::trunc);
    if (!out)
    {
        LOG_WARN("[CrashReporter] Failed to enqueue crash report: %s", queueFile.string().c_str());
        return false;
    }

    out << "schemaVersion=" << report.schemaVersion << "\n";
    out << "crashId=" << wstringToString(report.crashId) << "\n";
    out << "description=" << wstringToString(report.description) << "\n";
    out << "dumpPath=" << report.dumpPath.generic_string() << "\n";
    out << "metadataPath=" << report.metadataPath.generic_string() << "\n";
    out << "exceptionCode=" << report.exceptionCode << "\n";
    out << "exceptionAddress=" << pointerToString(report.exceptionAddress) << "\n";
    out << "processId=" << report.processId << "\n";
    out << "threadId=" << report.threadId << "\n";
    out << "timestampUtcMs=" << report.timestampUtcMilliseconds << "\n";
    return out.good();
}

void CrashReporter::dispatchPendingReportsLocked()
{
    if (!m_externalSink)
    {
        return;
    }

    if (m_queueDirectory.empty())
    {
        m_queueDirectory = resolveQueueDirectory();
    }

    std::error_code ec;
    std::filesystem::create_directories(m_queueDirectory, ec);

    std::vector<std::filesystem::path> queueFiles;
    for (const auto& entry : std::filesystem::directory_iterator(m_queueDirectory, ec))
    {
        if (ec || !entry.is_regular_file())
        {
            continue;
        }

        if (entry.path().extension() == L".crashmeta")
        {
            queueFiles.push_back(entry.path());
        }
    }

    ExternalCrashSink sink = m_externalSink;
    for (const auto& queueFile : queueFiles)
    {
        CrashReport report;
        if (!parseMetadataFile(queueFile, report))
        {
            LOG_WARN("[CrashReporter] Failed to parse pending crash report metadata: %s", queueFile.string().c_str());
            std::filesystem::remove(queueFile, ec);
            continue;
        }

        report.recoveredFromQueue = true;
        sink(report);
        std::filesystem::remove(queueFile, ec);
    }
}

std::filesystem::path CrashReporter::resolveDumpDirectory() const
{
    wchar_t localAppData[MAX_PATH]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    if (length > 0 && length < std::size(localAppData))
    {
        return std::filesystem::path(localAppData) / kCrashDirectoryName / kCrashSubDirectoryName;
    }

    return std::filesystem::current_path() / kCrashDirectoryName / kCrashSubDirectoryName;
}

std::filesystem::path CrashReporter::resolveQueueDirectory() const
{
    wchar_t localAppData[MAX_PATH]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    if (length > 0 && length < std::size(localAppData))
    {
        return std::filesystem::path(localAppData) / kCrashDirectoryName / kCrashQueueSubDirectoryName;
    }

    return std::filesystem::current_path() / kCrashDirectoryName / kCrashQueueSubDirectoryName;
}

std::wstring CrashReporter::buildCrashId() const
{
    SYSTEMTIME time{};
    GetSystemTime(&time);

    return std::format(L"{:04}{:02}{:02}_{:02}{:02}{:02}_{:03}_{}",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        GetCurrentProcessId());
}

std::filesystem::path CrashReporter::buildDumpPath() const
{
    return buildDumpPath(buildCrashId());
}

std::filesystem::path CrashReporter::buildDumpPath(const std::wstring& crashId) const
{
    std::filesystem::path directory = m_dumpDirectory.empty() ? resolveDumpDirectory() : m_dumpDirectory;
    return directory / (L"Crash_" + crashId + L".dmp");
}

std::filesystem::path CrashReporter::buildMetadataPath(const std::wstring& crashId) const
{
    std::filesystem::path directory = m_dumpDirectory.empty() ? resolveDumpDirectory() : m_dumpDirectory;
    return directory / (L"Crash_" + crashId + L".meta");
}

bool CrashReporter::parseMetadataFile(const std::filesystem::path& filePath, CrashReport& outReport) const
{
    std::ifstream in(filePath);
    if (!in)
    {
        return false;
    }

    outReport = {};
    std::string line;
    while (std::getline(in, line))
    {
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        const std::string key = trimAscii(std::string_view(line).substr(0, eq));
        const std::string value = trimAscii(std::string_view(line).substr(eq + 1));

        if (key == "schemaVersion")
        {
            tryParseUInt32(value, outReport.schemaVersion);
        }
        else if (key == "crashId")
        {
            outReport.crashId = stringToWstring(value);
        }
        else if (key == "description")
        {
            outReport.description = stringToWstring(value);
        }
        else if (key == "dumpPath")
        {
            outReport.dumpPath = std::filesystem::path(value);
        }
        else if (key == "metadataPath")
        {
            outReport.metadataPath = std::filesystem::path(value);
        }
        else if (key == "exceptionCode")
        {
            uint32_t code = 0;
            if (tryParseUInt32(value, code))
            {
                outReport.exceptionCode = static_cast<DWORD>(code);
            }
        }
        else if (key == "exceptionAddress")
        {
            void* pointerValue = nullptr;
            if (tryParsePointer(value, pointerValue))
            {
                outReport.exceptionAddress = pointerValue;
            }
        }
        else if (key == "processId")
        {
            uint32_t processId = 0;
            if (tryParseUInt32(value, processId))
            {
                outReport.processId = static_cast<DWORD>(processId);
            }
        }
        else if (key == "threadId")
        {
            uint32_t threadId = 0;
            if (tryParseUInt32(value, threadId))
            {
                outReport.threadId = static_cast<DWORD>(threadId);
            }
        }
        else if (key == "timestampUtcMs")
        {
            tryParseUInt64(value, outReport.timestampUtcMilliseconds);
        }
    }

    if (outReport.crashId.empty() || outReport.dumpPath.empty())
    {
        return false;
    }

    if (outReport.metadataPath.empty())
    {
        outReport.metadataPath = filePath;
    }

    return true;
}
