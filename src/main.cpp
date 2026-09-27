#include "app/Application.h"
#include "app/CrashHandler.h"
#include "app/MessageWindow.h"
#include "core/Log.h"
#include "settings/Settings.h"
#include "update/UpdateService.h"
#include "version.h"

#include <Windows.h>
#include <shellapi.h>

#include <exception>
#include <string_view>

namespace
{

constexpr wchar_t kSingleInstanceMutex[] = L"Local\\stat-wisp-6E48CF9D-44ED-4B20-AE74-6C7B52305FE4";

// Returns a handle to a newly created mutex, or nullptr while another instance
// owns it. A relaunch after an update waits for the previous process to exit.
HANDLE AcquireSingleInstance(bool waitForPrevious) noexcept
{
    const auto deadline = GetTickCount64() + (waitForPrevious ? 15000 : 0);
    for (;;)
    {
        HANDLE mutex = CreateMutexW(nullptr, FALSE, kSingleInstanceMutex);
        if (!mutex)
        {
            return nullptr;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return mutex;
        }
        CloseHandle(mutex);
        if (GetTickCount64() >= deadline)
        {
            return nullptr;
        }
        Sleep(200);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // Load DLLs only from System32 (portable copies often run from Downloads)
    // and end the process on heap corruption instead of continuing.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    try
    {
        bool configureOnly = false;
        bool relaunch = false;
        int argumentCount = 0;
        if (auto **arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount))
        {
            for (int index = 1; index < argumentCount; ++index)
            {
                const std::wstring_view argument(arguments[index]);
                configureOnly = configureOnly || argument == L"--configure-only";
                relaunch = relaunch || argument == L"--relaunch";
            }
            LocalFree(arguments);
        }

        const auto dataDirectory = statwisp::SettingsStore().Path().parent_path();
        statwisp::InitializeLog(dataDirectory);
        statwisp::InstallCrashHandler(dataDirectory);

        HANDLE singleInstance = AcquireSingleInstance(relaunch);
        if (!singleInstance)
        {
            if (const auto existing = FindWindowW(statwisp::MessageWindow::ClassName(), nullptr))
            {
                AllowSetForegroundWindow(ASFW_ANY);
                PostMessageW(existing, statwisp::kShowMenuMessage, 0, 0);
            }
            return 0;
        }

        // After an update the previous process may still be exiting for a moment.
        statwisp::UpdateService::CleanupAfterUpdate(dataDirectory / L"updates",
                                                    std::chrono::milliseconds(relaunch ? 5000 : 0));
        statwisp::LogInfo(std::wstring(L"Stat Wisp ") + STAT_WISP_VERSION_WSTRING + L" started (" +
                          (statwisp::UpdateService::IsInstalledEdition() ? L"installed" : L"portable") + L").");
        int result = 0;
        {
            statwisp::Application application(instance, configureOnly);
            result = application.Run();
        }
        statwisp::LogInfo(L"Stat Wisp exited with code " + std::to_wstring(result) + L".");
        CloseHandle(singleInstance);
        return result;
    }
    catch (const std::exception &)
    {
        statwisp::LogError(L"Stat Wisp stopped after an unexpected error.");
        return 1;
    }
}
