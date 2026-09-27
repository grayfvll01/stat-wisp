#include "app/CrashHandler.h"

#include "core/Log.h"

#include <Windows.h>
#include <DbgHelp.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <exception>

namespace statwisp
{
namespace
{

constexpr int kKeptDumps = 3;
constexpr DWORD kCrashExitCode = 0xE0057A75;

using MiniDumpWriteDumpFunction = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                                 PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
                                                 PMINIDUMP_CALLBACK_INFORMATION);

// Fixed storage: nothing below may allocate once the process is failing.
wchar_t dumpDirectory[4096]{};
volatile LONG dumping{};

// Static buffers keep the failing thread's stack use small (stack overflow).
wchar_t scratchPath[4400]{};
wchar_t dumpNames[32][MAX_PATH]{};

void PruneDumps() noexcept
{
    if (swprintf_s(scratchPath, L"%s\\crash-*.dmp", dumpDirectory) < 0)
    {
        return;
    }
    int count = 0;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(scratchPath, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (count < 32)
        {
            wcscpy_s(dumpNames[count++], data.cFileName);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    // Names embed a sortable UTC timestamp. Keep room for the dump about to be written.
    while (count > kKeptDumps - 1)
    {
        int oldest = 0;
        for (int index = 1; index < count; ++index)
        {
            if (wcscmp(dumpNames[index], dumpNames[oldest]) < 0)
            {
                oldest = index;
            }
        }
        if (swprintf_s(scratchPath, L"%s\\%s", dumpDirectory, dumpNames[oldest]) >= 0)
        {
            DeleteFileW(scratchPath);
        }
        wcscpy_s(dumpNames[oldest], dumpNames[--count]);
    }
}

void WriteDump(EXCEPTION_POINTERS *exception) noexcept
{
    if (dumpDirectory[0] == L'\0' || InterlockedExchange(&dumping, 1) != 0)
    {
        return;
    }
    HMODULE dbghelp = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    const auto writeDump =
        dbghelp ? reinterpret_cast<MiniDumpWriteDumpFunction>(GetProcAddress(dbghelp, "MiniDumpWriteDump")) : nullptr;
    if (!writeDump)
    {
        return;
    }
    CreateDirectoryW(dumpDirectory, nullptr);
    PruneDumps();

    SYSTEMTIME time{};
    GetSystemTime(&time);
    if (swprintf_s(scratchPath, L"%s\\crash-%04u%02u%02u-%02u%02u%02u.dmp", dumpDirectory, time.wYear, time.wMonth,
                   time.wDay, time.wHour, time.wMinute, time.wSecond) < 0)
    {
        return;
    }
    HANDLE file = CreateFileW(scratchPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION information{GetCurrentThreadId(), exception, FALSE};
    writeDump(GetCurrentProcess(), GetCurrentProcessId(), file,
              static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
              exception ? &information : nullptr, nullptr, nullptr);
    CloseHandle(file);
    // The heap may be damaged; logging is the last, optional step.
    LogError(std::wstring_view(L"Stat Wisp stopped unexpectedly. A crash report was saved beside this log."));
}

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS *exception)
{
    WriteDump(exception);
    return EXCEPTION_EXECUTE_HANDLER;
}

// Captures the current context through SEH so a dump has a useful stack.
[[noreturn]] void FailWithDump() noexcept
{
    __try
    {
        RaiseException(kCrashExitCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
    __except (WriteDump(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER)
    {
    }
    TerminateProcess(GetCurrentProcess(), kCrashExitCode);
    std::abort();
}

void TerminateHandler()
{
    FailWithDump();
}

void PureCallHandler()
{
    FailWithDump();
}

void InvalidParameterHandler(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
    FailWithDump();
}

} // namespace

void InstallCrashHandler(const std::filesystem::path &directory) noexcept
{
    const auto &native = directory.native();
    if (native.empty() || native.size() >= std::size(dumpDirectory))
    {
        return;
    }
    wcscpy_s(dumpDirectory, native.c_str());
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    std::set_terminate(TerminateHandler);
    _set_purecall_handler(PureCallHandler);
    _set_invalid_parameter_handler(InvalidParameterHandler);
}

} // namespace statwisp
