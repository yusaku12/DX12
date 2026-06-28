#include "pch.h"
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace
{
    constexpr const wchar_t* kCrashDirectoryName = L"DirectX12";
    constexpr const wchar_t* kCrashSubDirectoryName = L"CrashDumps";
}

void CrashReporter::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
    {
        return;
    }

    m_dumpDirectory = resolveDumpDirectory();
    std::error_code ec;
    std::filesystem::create_directories(m_dumpDirectory, ec);

    m_previousUnhandledExceptionFilter = SetUnhandledExceptionFilter(&CrashReporter::unhandledExceptionFilter);
    m_previousTerminateHandler = std::set_terminate(&CrashReporter::terminateHandler);
    m_initialized = true;
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
    std::lock_guard<std::mutex> lock(m_mutex);
    m_externalSink = std::move(sink);
}

void CrashReporter::setDumpDirectory(const std::filesystem::path& dumpDirectory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dumpDirectory = dumpDirectory;
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
    CrashReport report;
    report.description = description;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_dumpDirectory.empty())
        {
            m_dumpDirectory = resolveDumpDirectory();
        }

        std::error_code ec;
        std::filesystem::create_directories(m_dumpDirectory, ec);

        report.dumpPath = buildDumpPath();
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

    if (externalSink)
    {
        externalSink(report);
    }

    return true;
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

std::filesystem::path CrashReporter::buildDumpPath() const
{
    SYSTEMTIME time{};
    GetLocalTime(&time);

    std::filesystem::path directory = m_dumpDirectory.empty() ? resolveDumpDirectory() : m_dumpDirectory;
    std::wstring fileName = std::format(L"Crash_{:04}{:02}{:02}_{:02}{:02}{:02}_{}.dmp",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        GetCurrentProcessId());

    return directory / fileName;
}
