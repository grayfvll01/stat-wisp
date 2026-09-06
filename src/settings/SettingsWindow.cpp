#include "settings/SettingsWindow.h"

#include "resource.h"

#include <CommCtrl.h>

#include <array>
#include <string>

namespace statwisp
{
namespace
{

constexpr wchar_t kClassName[] = L"stat-wisp-settings";
constexpr int kMetricBase = 2000;
constexpr int kInterval = 2100;
constexpr int kStartup = 2101;
constexpr int kFahrenheit = 2102;
constexpr int kCompact = 2103;
constexpr int kRound = 2104;
constexpr int kExplanation = 2105;

struct WindowState
{
    HINSTANCE instance{};
    HWND window{};
    HWND owner{};
    Settings working;
    bool firstRun{};
    bool accepted{};
    HFONT font{};
    std::array<HWND, kMetricCount> metricControls{};
    HWND interval{};
    HWND start{};
    HWND explanation{};
};

HWND AddControl(WindowState &state, const wchar_t *className, const wchar_t *text, DWORD style, int x, int y, int width,
                int height, int id)
{
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, state.window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), state.instance, nullptr);
    if (control && state.font)
    {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), TRUE);
    }
    return control;
}

void RefreshStartState(WindowState &state)
{
    std::size_t selected = 0;
    for (const auto control : state.metricControls)
    {
        if (SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            ++selected;
        }
    }
    EnableWindow(state.start, selected != 0);
    ShowWindow(state.explanation, selected == 0 ? SW_SHOW : SW_HIDE);
}

void CreateControls(WindowState &state)
{
    constexpr int columns = 2;
    const int rows = static_cast<int>((kMetricCount + columns - 1) / columns);
    const int metricsBottom = 46 + rows * 27;
    const int intervalY = metricsBottom + 12;
    const int displayY = metricsBottom + 53;
    const int startupY = metricsBottom + 93;
    const int explanationY = metricsBottom + 129;
    const int buttonY = metricsBottom + 171;

    state.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    AddControl(state, L"STATIC",
               state.firstRun ? L"Choose the metrics to display in the notification area:" : L"Visible tray metrics:",
               SS_LEFT, 18, 16, 460, 22, -1);

    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        const int column = static_cast<int>(index) / rows;
        const int row = static_cast<int>(index) % rows;
        const int x = 18 + column * 235;
        const int y = 46 + row * 27;
        const auto type = static_cast<MetricType>(index);
        state.metricControls[index] =
            AddControl(state, L"BUTTON", Definition(type).name.data(), BS_AUTOCHECKBOX | WS_TABSTOP, x, y, 225, 23,
                       kMetricBase + static_cast<int>(index));
        SendMessageW(state.metricControls[index], BM_SETCHECK,
                     state.working.IsEnabled(type) ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    AddControl(state, L"STATIC", L"Update interval:", SS_LEFT, 18, intervalY, 110, 22, -1);
    state.interval = AddControl(state, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 132,
                                intervalY - 5, 155, 220, kInterval);
    constexpr std::array<std::pair<const wchar_t *, std::uint32_t>, 7> intervals{{
        {L"500 ms", 500},
        {L"1 second", 1000},
        {L"2 seconds", 2000},
        {L"5 seconds", 5000},
        {L"10 seconds", 10000},
        {L"30 seconds", 30000},
        {L"1 minute", 60000},
    }};
    for (std::size_t index = 0; index < intervals.size(); ++index)
    {
        const auto item =
            SendMessageW(state.interval, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(intervals[index].first));
        SendMessageW(state.interval, CB_SETITEMDATA, item, intervals[index].second);
        if (intervals[index].second == state.working.updateIntervalMs)
        {
            SendMessageW(state.interval, CB_SETCURSEL, item, 0);
        }
    }
    if (SendMessageW(state.interval, CB_GETCURSEL, 0, 0) == CB_ERR)
    {
        SendMessageW(state.interval, CB_SETCURSEL, 1, 0);
    }

    AddControl(state, L"STATIC", L"Display:", SS_LEFT, 18, displayY + 4, 80, 22, -1);
    const auto fahrenheit =
        AddControl(state, L"BUTTON", L"Fahrenheit", BS_AUTOCHECKBOX | WS_TABSTOP, 100, displayY, 105, 23, kFahrenheit);
    const auto compact =
        AddControl(state, L"BUTTON", L"Compact labels", BS_AUTOCHECKBOX | WS_TABSTOP, 215, displayY, 115, 23, kCompact);
    const auto round =
        AddControl(state, L"BUTTON", L"Round values", BS_AUTOCHECKBOX | WS_TABSTOP, 340, displayY, 105, 23, kRound);
    SendMessageW(fahrenheit, BM_SETCHECK, state.working.fahrenheit ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(compact, BM_SETCHECK, state.working.compactLabels ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(round, BM_SETCHECK, state.working.roundValues ? BST_CHECKED : BST_UNCHECKED, 0);

    const auto startup = AddControl(state, L"BUTTON", L"Start Stat Wisp with Windows", BS_AUTOCHECKBOX | WS_TABSTOP,
                                    18, startupY, 240, 23, kStartup);
    SendMessageW(startup, BM_SETCHECK, state.working.startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);

    state.explanation =
        AddControl(state, L"STATIC", L"Select at least one metric. Stat Wisp exists only in the notification area.",
                   SS_LEFT, 18, explanationY, 460, 32, kExplanation);
    state.start = AddControl(state, L"BUTTON", state.firstRun ? L"Start" : L"Save", BS_DEFPUSHBUTTON | WS_TABSTOP, 310,
                             buttonY, 78, 27, IDOK);
    AddControl(state, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 395, buttonY, 78, 27, IDCANCEL);
    RefreshStartState(state);
}

void ReadControls(WindowState &state)
{
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        state.working.enabled[index] = SendMessageW(state.metricControls[index], BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    const auto selected = SendMessageW(state.interval, CB_GETCURSEL, 0, 0);
    if (selected != CB_ERR)
    {
        state.working.updateIntervalMs =
            static_cast<std::uint32_t>(SendMessageW(state.interval, CB_GETITEMDATA, selected, 0));
    }
    state.working.startWithWindows = IsDlgButtonChecked(state.window, kStartup) == BST_CHECKED;
    state.working.fahrenheit = IsDlgButtonChecked(state.window, kFahrenheit) == BST_CHECKED;
    state.working.compactLabels = IsDlgButtonChecked(state.window, kCompact) == BST_CHECKED;
    state.working.roundValues = IsDlgButtonChecked(state.window, kRound) == BST_CHECKED;
    state.working.firstRunCompleted = true;
    state.working.Normalize();
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        state = static_cast<WindowState *>(create->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    switch (message)
    {
    case WM_CREATE:
        CreateControls(*state);
        return 0;
    case WM_COMMAND: {
        const auto id = LOWORD(wParam);
        if (id >= kMetricBase && id < kMetricBase + static_cast<int>(kMetricCount) && HIWORD(wParam) == BN_CLICKED)
        {
            RefreshStartState(*state);
            return 0;
        }
        if (id == IDOK)
        {
            ReadControls(*state);
            if (state->working.EnabledCount() == 0)
            {
                RefreshStartState(*state);
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            state->accepted = true;
            DestroyWindow(window);
            return 0;
        }
        if (id == IDCANCEL)
        {
            DestroyWindow(window);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        state->window = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool RegisterWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_STAT_WISP));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kClassName;
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

} // namespace

bool SettingsWindow::Show(HINSTANCE instance, HWND owner, Settings &settings, bool firstRun)
{
    if (!RegisterWindowClass(instance))
    {
        return false;
    }
    WindowState state;
    state.instance = instance;
    state.owner = owner;
    state.working = settings;
    state.firstRun = firstRun;

    if (owner)
    {
        EnableWindow(owner, FALSE);
    }
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, kClassName,
                                  firstRun ? L"Stat Wisp — first setup" : L"Stat Wisp settings",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 510, 590,
                                  owner, nullptr, instance, &state);
    if (!window)
    {
        if (owner)
        {
            EnableWindow(owner, TRUE);
        }
        return false;
    }
    RECT windowRect{};
    GetWindowRect(window, &windowRect);
    const auto width = windowRect.right - windowRect.left;
    const auto height = windowRect.bottom - windowRect.top;
    const auto x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const auto y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    int messageResult = 1;
    while (state.window && (messageResult = GetMessageW(&message, nullptr, 0, 0)) > 0)
    {
        if (!IsDialogMessageW(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (state.window)
    {
        DestroyWindow(state.window);
    }
    if (messageResult == 0)
    {
        // Preserve WM_QUIT for the application's outer message loop.
        PostQuitMessage(static_cast<int>(message.wParam));
    }
    if (owner)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    if (state.accepted)
    {
        settings = state.working;
    }
    return state.accepted;
}

} // namespace statwisp
