#include "core/Log.h"

#include <Windows.h>

#include <cstdio>
#include <string>

namespace statwisp
{
namespace
{

constexpr ULONGLONG kMaxLogBytes = 256 * 1024;

SRWLOCK logLock = SRWLOCK_INIT;
std::filesystem::path logPath;

class ExclusiveLock final
{
  public:
    ExclusiveLock() noexcept
    {
        AcquireSRWLockExclusive(&logLock);
    }
    ~ExclusiveLock()
    {
        ReleaseSRWLockExclusive(&logLock);
    }
    ExclusiveLock(const ExclusiveLock &) = delete;
    ExclusiveLock &operator=(const ExclusiveLock &) = delete;
};

const wchar_t *LevelName(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Warning:
        return L"WARN ";
    case LogLevel::Error:
        return L"ERROR";
    default:
        return L"INFO ";
    }
}

} // namespace

void InitializeLog(const std::filesystem::path &directory) noexcept
{
    try
    {
        ExclusiveLock lock;
        logPath = directory.empty() ? std::filesystem::path{} : directory / L"stat-wisp.log";
    }
    catch (...)
    {
    }
}

std::filesystem::path LogFilePath() noexcept
{
    try
    {
        ExclusiveLock lock;
        return logPath;
    }
    catch (...)
    {
        return {};
    }
}

void WriteLog(LogLevel level, std::wstring_view message) noexcept
{
    try
    {
        ExclusiveLock lock;
        if (logPath.empty())
        {
            return;
        }
        std::error_code error;
        std::filesystem::create_directories(logPath.parent_path(), error);

        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (GetFileAttributesExW(logPath.c_str(), GetFileExInfoStandard, &attributes) &&
            ((static_cast<ULONGLONG>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow) >= kMaxLogBytes)
        {
            auto previous = logPath;
            previous += L".1";
            MoveFileExW(logPath.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        SYSTEMTIME time{};
        GetSystemTime(&time);
        wchar_t prefix[48]{};
        swprintf_s(prefix, L"%04u-%02u-%02uT%02u:%02u:%02uZ %s ", time.wYear, time.wMonth, time.wDay, time.wHour,
                   time.wMinute, time.wSecond, LevelName(level));
        std::wstring line(prefix);
        for (const auto character : message)
        {
            line.push_back(character == L'\r' || character == L'\n' ? L' ' : character);
        }
        line += L"\r\n";

        const auto bytes =
            WideCharToMultiByte(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0)
        {
            return;
        }
        std::string utf8(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), utf8.data(), bytes, nullptr,
                            nullptr);
        HANDLE file = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
            CloseHandle(file);
        }
    }
    catch (...)
    {
    }
}

} // namespace statwisp
