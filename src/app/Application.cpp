#include "app/Application.h"

#include "core/Log.h"
#include "resource.h"
#include "settings/SettingsWindow.h"
#include "startup/StartupManager.h"
#include "version.h"

#include <CommCtrl.h>
#include <Objbase.h>
#include <shellapi.h>
#include <windowsx.h>

#include <array>
#include <string>

namespace statwisp
{
namespace
{

constexpr UINT kMetricCommandBase = 4100;
constexpr UINT kIntervalCommandBase = 4200;
constexpr UINT kCelsiusCommand = 4300;
constexpr UINT kFahrenheitCommand = 4301;
constexpr UINT kCompactCommand = 4302;
constexpr UINT kRoundCommand = 4303;
constexpr UINT kStartupCommand = 4400;
constexpr UINT kPauseCommand = 4500;
constexpr UINT kRestartCommand = 4501;
constexpr UINT kSettingsCommand = 4502;
constexpr UINT kAboutCommand = 4503;
constexpr UINT kExitCommand = 4504;
constexpr UINT kCheckUpdatesCommand = 4505;
constexpr UINT kInstallUpdateCommand = 4506;
constexpr UINT_PTR kTrayRetryTimer = 1;
constexpr UINT kTrayRetryMilliseconds = 2000;

void AddMenuItem(HMENU menu, UINT command, const wchar_t *text, bool checked = false, bool enabled = true)
{
    UINT flags = MF_STRING;
    if (checked)
    {
        flags |= MF_CHECKED;
    }
    if (!enabled)
    {
        flags |= MF_GRAYED;
    }
    AppendMenuW(menu, flags, command, text);
}

} // namespace

Application::Application(HINSTANCE instance, bool configureOnly) : instance_(instance), configureOnly_(configureOnly)
{
}

Application::~Application()
{
    updates_.Stop();
    monitor_.Stop();
    tray_.reset();
}

int Application::Run()
{
    // The UI thread opens links and the About dialog through the shell.
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    struct ComScope
    {
        HRESULT result;
        ~ComScope()
        {
            if (SUCCEEDED(result))
            {
                CoUninitialize();
            }
        }
    } comScope{com};

    bool recovered = false;
    settings_ = settingsStore_.Load(&recovered);
    if (recovered)
    {
        // Keep the unreadable file for inspection instead of overwriting it silently.
        auto backup = settingsStore_.Path();
        backup += L".bak";
        CopyFileW(settingsStore_.Path().c_str(), backup.c_str(), FALSE);
        LogWarning(L"Settings could not be read. A copy was kept as settings.ini.bak and setup was shown.");
    }
    if (settings_.firstRunCompleted)
    {
        // Uninstalling or another tool can remove the startup entry; show the real state.
        settings_.startWithWindows = StartupManager::IsEnabled();
    }
    if (!settingsStore_.Exists() || recovered || !settings_.firstRunCompleted || settings_.EnabledCount() == 0)
    {
        settings_.firstRunCompleted = false;
        if (!SettingsWindow::Show(instance_, nullptr, settings_, true))
        {
            return 2;
        }
        settings_.firstRunCompleted = true;
        if (settings_.EnabledCount() == 0)
        {
            return 0;
        }
        if (settings_.startWithWindows && !StartupManager::SetEnabled(true))
        {
            settings_.startWithWindows = false;
            MessageBoxW(nullptr, L"Stat Wisp could not create the per-user startup entry.", L"Stat Wisp",
                        MB_OK | MB_ICONWARNING);
        }
        else if (!settings_.startWithWindows)
        {
            (void)StartupManager::SetEnabled(false);
        }
        if (!settingsStore_.Save(settings_))
        {
            MessageBoxW(nullptr,
                        configureOnly_ ? L"Settings could not be saved. Setup was not completed."
                                       : L"Settings could not be saved. The current session will continue with them.",
                        L"Stat Wisp", MB_OK | MB_ICONWARNING);
            if (configureOnly_)
            {
                return 1;
            }
        }
    }

    if (configureOnly_)
    {
        return settings_.firstRunCompleted && settings_.EnabledCount() != 0 ? 0 : 2;
    }

    if (!messageWindow_.Create(instance_, *this))
    {
        MessageBoxW(nullptr, L"Stat Wisp could not create its notification window.", L"Stat Wisp",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    tray_ = std::make_unique<TrayManager>(messageWindow_.Handle(), renderer_);
    SyncTray(false);
    monitor_.Start(messageWindow_.Handle(), kSnapshotReadyMessage, settings_);
    updates_.Start(messageWindow_.Handle(), kUpdateStateMessage, settings_.checkForUpdates,
                   settingsStore_.Path().parent_path() / L"updates");

    MSG message{};
    int result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    updates_.Stop();
    monitor_.Stop();
    tray_.reset();
    messageWindow_.Destroy();
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}

LRESULT Application::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, bool &handled)
{
    if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_)
    {
        if (tray_ && !tray_->Recreate(settings_, latest_))
        {
            SyncTray(false);
        }
        handled = true;
        return 0;
    }
    switch (message)
    {
    case kSnapshotReadyMessage:
        latest_ = monitor_.LatestSnapshot();
        SyncTray(false);
        handled = true;
        return 0;
    case kUpdateStateMessage:
        HandleUpdateState();
        handled = true;
        return 0;
    case WM_TIMER:
        if (wParam == kTrayRetryTimer)
        {
            SyncTray(false);
            handled = true;
            return 0;
        }
        break;
    case kTrayCallbackMessage: {
        const auto event = LOWORD(lParam);
        if (event == NIN_BALLOONUSERCLICK)
        {
            const auto state = updates_.State();
            if (state.availableVersion &&
                (state.status == UpdateStatus::Available || state.status == UpdateStatus::Failed))
            {
                HandleCommand(kInstallUpdateCommand);
            }
            handled = true;
            return 0;
        }
        if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP || event == NIN_SELECT || event == NIN_KEYSELECT)
        {
            POINT point{};
            if (event == WM_CONTEXTMENU)
            {
                point.x = GET_X_LPARAM(wParam);
                point.y = GET_Y_LPARAM(wParam);
            }
            else
            {
                GetCursorPos(&point);
            }
            ShowContextMenu(point);
        }
        handled = true;
        return 0;
    }
    case kShowMenuMessage: {
        POINT point{};
        GetCursorPos(&point);
        ShowContextMenu(point);
        handled = true;
        return 0;
    }
    case WM_COMMAND:
        HandleCommand(LOWORD(wParam));
        handled = true;
        return 0;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
    case WM_DISPLAYCHANGE:
        SyncTray(true);
        handled = true;
        return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND)
        {
            suspended_ = true;
            monitor_.SetPaused(true);
        }
        else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND)
        {
            suspended_ = false;
            monitor_.RestartProviders();
            monitor_.SetPaused(paused_);
        }
        handled = true;
        return TRUE;
    case WM_QUERYENDSESSION:
        handled = true;
        return TRUE;
    case WM_ENDSESSION:
        if (wParam)
        {
            Exit();
        }
        handled = true;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        handled = true;
        return 0;
    default:
        break;
    }
    (void)window;
    handled = false;
    return 0;
}

void Application::ShowContextMenu(POINT point)
{
    if (settingsOpen_)
    {
        SetForegroundWindow(GetLastActivePopup(messageWindow_.Handle()));
        return;
    }
    HMENU menu = CreatePopupMenu();
    HMENU metrics = CreatePopupMenu();
    HMENU intervals = CreatePopupMenu();
    HMENU display = CreatePopupMenu();
    if (!menu || !metrics || !intervals || !display)
    {
        if (menu)
            DestroyMenu(menu);
        if (metrics)
            DestroyMenu(metrics);
        if (intervals)
            DestroyMenu(intervals);
        if (display)
            DestroyMenu(display);
        return;
    }

    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Stat Wisp  " STAT_WISP_VERSION_WSTRING);
    const auto update = updates_.State();
    const bool updatesManaged = update.status == UpdateStatus::Disabled;
    if (update.availableVersion && !updatesManaged)
    {
        if (update.status == UpdateStatus::Downloading)
        {
            const auto text = L"Downloading update… " + std::to_wstring(update.progressPercent) + L"%";
            AddMenuItem(menu, kInstallUpdateCommand, text.c_str(), false, false);
        }
        else
        {
            const auto text = L"Install update " + update.availableVersion->ToWString();
            AddMenuItem(menu, kInstallUpdateCommand, text.c_str(), false, update.status != UpdateStatus::Installing);
            SetMenuDefaultItem(menu, kInstallUpdateCommand, FALSE);
        }
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    for (const auto type : settings_.order)
    {
        const bool enabled = settings_.IsEnabled(type);
        AddMenuItem(metrics, kMetricCommandBase + static_cast<UINT>(MetricIndex(type)), Definition(type).name.data(),
                    enabled, !(enabled && settings_.EnabledCount() == 1));
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(metrics), L"Metrics");

    constexpr std::array<std::pair<const wchar_t *, std::uint32_t>, 7> choices{{
        {L"500 ms", 500},
        {L"1 second", 1000},
        {L"2 seconds", 2000},
        {L"5 seconds", 5000},
        {L"10 seconds", 10000},
        {L"30 seconds", 30000},
        {L"1 minute", 60000},
    }};
    for (std::size_t index = 0; index < choices.size(); ++index)
    {
        AddMenuItem(intervals, kIntervalCommandBase + static_cast<UINT>(index), choices[index].first,
                    settings_.updateIntervalMs == choices[index].second);
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(intervals), L"Update interval");

    AddMenuItem(display, kCelsiusCommand, L"Celsius", !settings_.fahrenheit);
    AddMenuItem(display, kFahrenheitCommand, L"Fahrenheit", settings_.fahrenheit);
    AppendMenuW(display, MF_SEPARATOR, 0, nullptr);
    AddMenuItem(display, kCompactCommand, L"Compact labels", settings_.compactLabels);
    AddMenuItem(display, kRoundCommand, L"Round values", settings_.roundValues);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(display), L"Display");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AddMenuItem(menu, kStartupCommand, L"Start with Windows", settings_.startWithWindows);
    AddMenuItem(menu, kPauseCommand, paused_ ? L"Resume monitoring" : L"Pause monitoring", paused_);
    AddMenuItem(menu, kRestartCommand, L"Restart monitoring");
    AddMenuItem(menu, kSettingsCommand, L"Settings…");
    const bool updateBusy = update.status == UpdateStatus::Checking || update.status == UpdateStatus::Downloading ||
                            update.status == UpdateStatus::Installing;
    AddMenuItem(menu, kCheckUpdatesCommand,
                updatesManaged ? L"Updates managed by your organization"
                               : (update.status == UpdateStatus::Checking ? L"Checking for updates…"
                                                                           : L"Check for updates"),
                false, !updatesManaged && !updateBusy);
    AddMenuItem(menu, kAboutCommand, L"About");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AddMenuItem(menu, kExitCommand, L"Exit Stat Wisp");

    SetForegroundWindow(messageWindow_.Handle());
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, point.x, point.y,
                                          messageWindow_.Handle(), nullptr);
    DestroyMenu(menu);
    PostMessageW(messageWindow_.Handle(), WM_NULL, 0, 0);
    if (command != 0)
    {
        HandleCommand(command);
    }
}

void Application::HandleCommand(UINT command)
{
    // Shell callbacks and posted commands still arrive while the settings
    // window's modal loop is running. Keep its working copy authoritative.
    if (exiting_ || (settingsOpen_ && command != kExitCommand))
    {
        return;
    }
    if (command >= kMetricCommandBase && command < kMetricCommandBase + kMetricCount)
    {
        auto next = settings_;
        const auto type = static_cast<MetricType>(command - kMetricCommandBase);
        if (next.IsEnabled(type) && next.EnabledCount() == 1)
        {
            MessageBeep(MB_ICONWARNING);
            return;
        }
        next.SetEnabled(type, !next.IsEnabled(type));
        ApplySettings(next);
        return;
    }
    if (command >= kIntervalCommandBase && command < kIntervalCommandBase + kAllowedUpdateIntervals.size())
    {
        auto next = settings_;
        next.updateIntervalMs = kAllowedUpdateIntervals[command - kIntervalCommandBase];
        ApplySettings(next);
        return;
    }
    auto next = settings_;
    switch (command)
    {
    case kCelsiusCommand:
        next.fahrenheit = false;
        ApplySettings(next);
        break;
    case kFahrenheitCommand:
        next.fahrenheit = true;
        ApplySettings(next);
        break;
    case kCompactCommand:
        next.compactLabels = !next.compactLabels;
        ApplySettings(next);
        break;
    case kRoundCommand:
        next.roundValues = !next.roundValues;
        ApplySettings(next);
        break;
    case kStartupCommand:
        next.startWithWindows = !next.startWithWindows;
        ApplySettings(next);
        break;
    case kPauseCommand:
        paused_ = !paused_;
        monitor_.SetPaused(paused_ || suspended_);
        break;
    case kRestartCommand:
        paused_ = false;
        monitor_.RestartProviders();
        monitor_.SetPaused(suspended_);
        break;
    case kSettingsCommand:
        OpenSettings();
        break;
    case kCheckUpdatesCommand:
        updates_.CheckNow();
        break;
    case kInstallUpdateCommand:
        updates_.InstallUpdate();
        break;
    case kAboutCommand:
        ShowAbout();
        break;
    case kExitCommand:
        Exit();
        break;
    default:
        break;
    }
}

void Application::ApplySettings(Settings next)
{
    next.Normalize();
    if (next.EnabledCount() == 0)
    {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    next.firstRunCompleted = true;
    if (next.startWithWindows != settings_.startWithWindows && !StartupManager::SetEnabled(next.startWithWindows))
    {
        next.startWithWindows = settings_.startWithWindows;
        MessageBoxW(nullptr, L"The per-user Windows startup setting could not be changed.", L"Stat Wisp",
                    MB_OK | MB_ICONWARNING);
    }
    const bool updateChecksChanged = next.checkForUpdates != settings_.checkForUpdates;
    settings_ = next;
    if (updateChecksChanged)
    {
        updates_.SetAutomaticChecks(settings_.checkForUpdates);
    }
    if (!settingsStore_.Save(settings_))
    {
        MessageBoxW(nullptr, L"Settings could not be saved. The current session will continue with them.",
                    L"Stat Wisp", MB_OK | MB_ICONWARNING);
    }
    SyncTray(true);
    monitor_.UpdateSettings(settings_);
}

void Application::OpenSettings()
{
    auto next = settings_;
    settingsOpen_ = true;
    const bool accepted = SettingsWindow::Show(instance_, messageWindow_.Handle(), next, false);
    settingsOpen_ = false;
    if (accepted && !exiting_)
    {
        ApplySettings(next);
    }
}

void Application::ShowAbout()
{
    const wchar_t *footer = UpdateService::DisabledByPolicy()
                                ? L"No telemetry. Updates are managed by your organization."
                            : settings_.checkForUpdates
                                ? L"No telemetry. The only network request is the update check to GitHub."
                                : L"No telemetry. Automatic update checks are off.";
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hInstance = instance_;
    config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = L"About Stat Wisp";
    config.pszMainIcon = MAKEINTRESOURCEW(IDI_STAT_WISP);
    config.pszMainInstruction = L"Stat Wisp " STAT_WISP_VERSION_WSTRING;
    config.pszContent = L"Your PC's vitals, quietly in the Windows tray.\n\n"
                        L"<a href=\"https://github.com/grayfvll01/stat-wisp\">github.com/grayfvll01/stat-wisp</a>\n"
                        L"Free and open source under the MIT License.";
    config.pszFooter = footer;
    config.pszFooterIcon = TD_INFORMATION_ICON;
    config.pfCallback = [](HWND, UINT notification, WPARAM, LPARAM lParam, LONG_PTR) -> HRESULT {
        if (notification == TDN_HYPERLINK_CLICKED)
        {
            ShellExecuteW(nullptr, L"open", reinterpret_cast<const wchar_t *>(lParam), nullptr, nullptr,
                          SW_SHOWNORMAL);
        }
        return S_OK;
    };
    if (FAILED(TaskDialogIndirect(&config, nullptr, nullptr, nullptr)))
    {
        MessageBoxW(nullptr, L"Stat Wisp " STAT_WISP_VERSION_WSTRING L"\n\nhttps://github.com/grayfvll01/stat-wisp",
                    L"About Stat Wisp", MB_OK | MB_ICONINFORMATION);
    }
}

void Application::HandleUpdateState()
{
    const auto state = updates_.State();
    if (state.sequence == handledUpdateSequence_ || exiting_)
    {
        return;
    }
    handledUpdateSequence_ = state.sequence;
    const auto balloon = [this](const std::wstring &title, const std::wstring &text) {
        if (tray_)
        {
            (void)tray_->ShowBalloon(title, text);
        }
    };
    switch (state.status)
    {
    case UpdateStatus::Available:
        // Announce each version once per session unless the user asked.
        if (state.availableVersion && (state.userInitiated || announcedVersion_ != state.availableVersion))
        {
            announcedVersion_ = state.availableVersion;
            balloon(L"Stat Wisp " + state.availableVersion->ToWString() + L" is available",
                    L"Click here to install it, or choose Install update in the tray menu.");
        }
        break;
    case UpdateStatus::UpToDate:
        if (state.userInitiated)
        {
            balloon(L"Stat Wisp is up to date", L"You have the latest version, " STAT_WISP_VERSION_WSTRING L".");
        }
        break;
    case UpdateStatus::Failed:
        if (state.userInitiated)
        {
            balloon(L"Update not completed", state.message);
        }
        break;
    case UpdateStatus::Installing:
        LogInfo(L"Exiting so the update can be installed.");
        Exit();
        break;
    default:
        break;
    }
}

void Application::SyncTray(bool refresh)
{
    if (!tray_ || exiting_)
    {
        return;
    }
    const bool complete = refresh ? tray_->Refresh(settings_, latest_) : tray_->Sync(settings_, latest_);
    if (!complete && !trayRetryPending_)
    {
        // Explorer may not be ready yet (early logon) or may be restarting.
        trayRetryPending_ =
            SetTimer(messageWindow_.Handle(), kTrayRetryTimer, kTrayRetryMilliseconds, nullptr) != 0;
    }
    else if (complete && trayRetryPending_)
    {
        KillTimer(messageWindow_.Handle(), kTrayRetryTimer);
        trayRetryPending_ = false;
    }
}

void Application::Exit()
{
    if (exiting_)
    {
        return;
    }
    exiting_ = true;
    if (trayRetryPending_)
    {
        KillTimer(messageWindow_.Handle(), kTrayRetryTimer);
        trayRetryPending_ = false;
    }
    if (tray_)
    {
        tray_->RemoveAll();
    }
    updates_.Stop();
    monitor_.Stop();
    PostQuitMessage(0);
}

} // namespace statwisp
