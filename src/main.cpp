#include "app/Application.h"
#include "app/MessageWindow.h"

#include <Windows.h>
#include <shellapi.h>

#include <string_view>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool configureOnly = false;
    int argumentCount = 0;
    if (auto **arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount))
    {
        for (int index = 1; index < argumentCount; ++index)
        {
            if (std::wstring_view(arguments[index]) == L"--configure-only")
            {
                configureOnly = true;
            }
        }
        LocalFree(arguments);
    }

    HANDLE singleInstance = CreateMutexW(nullptr, FALSE, L"Local\\gate-monitor-6E48CF9D-44ED-4B20-AE74-6C7B52305FE4");
    if (!singleInstance)
    {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (const auto existing = FindWindowW(gate::MessageWindow::ClassName(), nullptr))
        {
            PostMessageW(existing, gate::kShowMenuMessage, 0, 0);
        }
        CloseHandle(singleInstance);
        return 0;
    }

    gate::Application application(instance, configureOnly);
    const auto result = application.Run();
    CloseHandle(singleInstance);
    return result;
}
