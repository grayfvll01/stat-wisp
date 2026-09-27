#include "settings/SettingsWindow.h"

#include "resource.h"
#include "update/UpdateService.h"

#include <CommCtrl.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

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
constexpr int kCheckUpdates = 2106;

// Layout in device-independent pixels (96 DPI), scaled for each monitor.
constexpr int kMargin = 20;
constexpr int kColumn = 232;
constexpr int kRow = 26;
constexpr int kClientWidth = kMargin * 2 + kColumn * 2;
constexpr int kFooterHeight = 58;
constexpr int kButtonWidth = 92;
constexpr int kButtonHeight = 30;
constexpr DWORD kStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
constexpr DWORD kExStyle = WS_EX_APPWINDOW;

constexpr std::array<std::pair<const wchar_t *, std::uint32_t>, 7> kIntervals{{
    {L"500 ms", 500},
    {L"1 second", 1000},
    {L"2 seconds", 2000},
    {L"5 seconds", 5000},
    {L"10 seconds", 10000},
    {L"30 seconds", 30000},
    {L"1 minute", 60000},
}};

struct Placed
{
    HWND window;
    RECT bounds; // DIPs
    bool heading;
};

struct WindowState
{
    HINSTANCE instance{};
    HWND window{};
    HWND owner{};
    Settings working;
    bool firstRun{};
    bool accepted{};
    bool updatesManaged{};
    UINT dpi{USER_DEFAULT_SCREEN_DPI};
    HFONT font{};
    HFONT headingFont{};
    std::vector<Placed> controls;
    std::array<HWND, kMetricCount> metricControls{};
    HWND interval{};
    HWND start{};
    HWND explanation{};
    int footerTop{};

    WindowState() = default;
    WindowState(const WindowState &) = delete;
    WindowState &operator=(const WindowState &) = delete;
    ~WindowState()
    {
        DeleteFonts();
    }
    void DeleteFonts() noexcept
    {
        if (font)
        {
            DeleteObject(font);
            font = nullptr;
        }
        if (headingFont)
        {
            DeleteObject(headingFont);
            headingFont = nullptr;
        }
    }
};

int Scale(const WindowState &state, int value) noexcept
{
    return MulDiv(value, static_cast<int>(state.dpi), USER_DEFAULT_SCREEN_DPI);
}

void CreateFonts(WindowState &state)
{
    state.DeleteFonts();
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    LOGFONTW base{};
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, state.dpi))
    {
        base = metrics.lfMessageFont;
    }
    else
    {
        base.lfHeight = -MulDiv(9, static_cast<int>(state.dpi), 72);
        base.lfWeight = FW_NORMAL;
        base.lfCharSet = DEFAULT_CHARSET;
        base.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(base.lfFaceName, L"Segoe UI");
    }
    state.font = CreateFontIndirectW(&base);
    auto heading = base;
    heading.lfWeight = FW_SEMIBOLD;
    heading.lfHeight = MulDiv(base.lfHeight, 10, 9);
    state.headingFont = CreateFontIndirectW(&heading);
}

HWND AddControl(WindowState &state, const wchar_t *className, const wchar_t *text, DWORD style, int x, int y,
                int width, int height, int id, bool heading = false)
{
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, state.window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), state.instance, nullptr);
    if (control)
    {
        state.controls.push_back({control, RECT{x, y, x + width, y + height}, heading});
    }
    return control;
}

void ApplyLayout(WindowState &state)
{
    for (const auto &placed : state.controls)
    {
        SendMessageW(placed.window, WM_SETFONT,
                     reinterpret_cast<WPARAM>(placed.heading ? state.headingFont : state.font), FALSE);
        SetWindowPos(placed.window, nullptr, Scale(state, placed.bounds.left), Scale(state, placed.bounds.top),
                     Scale(state, placed.bounds.right - placed.bounds.left),
                     Scale(state, placed.bounds.bottom - placed.bounds.top), SWP_NOZORDER | SWP_NOACTIVATE);
    }
    InvalidateRect(state.window, nullptr, TRUE);
}

SIZE WindowSize(const WindowState &state) noexcept
{
    RECT rect{0, 0, Scale(state, kClientWidth), Scale(state, state.footerTop + kFooterHeight)};
    AdjustWindowRectExForDpi(&rect, kStyle, FALSE, kExStyle, state.dpi);
    return {rect.right - rect.left, rect.bottom - rect.top};
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

void SetCheck(HWND control, bool checked) noexcept
{
    SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void CreateControls(WindowState &state)
{
    constexpr int columns = 2;
    constexpr int rows = static_cast<int>((kMetricCount + columns - 1) / columns);
    constexpr DWORD checkbox = BS_AUTOCHECKBOX | WS_TABSTOP;
    int y = 16;

    AddControl(state, L"STATIC", L"Metrics", SS_LEFT | SS_NOPREFIX, kMargin, y, kColumn * 2, 22, -1, true);
    y += 26;
    AddControl(state, L"STATIC",
               state.firstRun ? L"Choose what to show. Each metric gets its own icon beside the clock."
                              : L"Each selected metric gets its own icon beside the clock.",
               SS_LEFT | SS_NOPREFIX, kMargin, y, kColumn * 2, 20, -1);
    y += 28;
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        const int column = static_cast<int>(index) / rows;
        const int row = static_cast<int>(index) % rows;
        const auto type = static_cast<MetricType>(index);
        state.metricControls[index] =
            AddControl(state, L"BUTTON", Definition(type).name.data(), checkbox, kMargin + column * kColumn,
                       y + row * kRow, kColumn - 12, 22, kMetricBase + static_cast<int>(index));
        SetCheck(state.metricControls[index], state.working.IsEnabled(type));
    }
    y += rows * kRow + 14;

    AddControl(state, L"STATIC", L"Display", SS_LEFT | SS_NOPREFIX, kMargin, y, kColumn * 2, 22, -1, true);
    y += 32;
    AddControl(state, L"STATIC", L"Update &interval:", SS_LEFT, kMargin, y + 4, 110, 20, -1);
    state.interval = AddControl(state, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                                kMargin + 116, y, 160, 220, kInterval);
    for (const auto &[label, milliseconds] : kIntervals)
    {
        const auto item = SendMessageW(state.interval, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        SendMessageW(state.interval, CB_SETITEMDATA, item, milliseconds);
        if (milliseconds == state.working.updateIntervalMs)
        {
            SendMessageW(state.interval, CB_SETCURSEL, item, 0);
        }
    }
    if (SendMessageW(state.interval, CB_GETCURSEL, 0, 0) == CB_ERR)
    {
        SendMessageW(state.interval, CB_SETCURSEL, 1, 0);
    }
    y += 36;
    SetCheck(AddControl(state, L"BUTTON", L"&Fahrenheit", checkbox, kMargin, y, 120, 22, kFahrenheit),
             state.working.fahrenheit);
    SetCheck(AddControl(state, L"BUTTON", L"&Compact labels", checkbox, kMargin + 132, y, 150, 22, kCompact),
             state.working.compactLabels);
    SetCheck(AddControl(state, L"BUTTON", L"&Round values", checkbox, kMargin + 294, y, 150, 22, kRound),
             state.working.roundValues);
    y += kRow + 16;

    AddControl(state, L"STATIC", L"General", SS_LEFT | SS_NOPREFIX, kMargin, y, kColumn * 2, 22, -1, true);
    y += 30;
    SetCheck(AddControl(state, L"BUTTON", L"Start Stat Wisp with &Windows", checkbox, kMargin, y, kColumn * 2, 22,
                        kStartup),
             state.working.startWithWindows);
    y += kRow;
    const auto updates =
        AddControl(state, L"BUTTON",
                   state.updatesManaged ? L"Check for updates (managed by your organization)"
                                        : L"Check for &updates automatically",
                   checkbox, kMargin, y, kColumn * 2, 22, kCheckUpdates);
    SetCheck(updates, !state.updatesManaged && state.working.checkForUpdates);
    EnableWindow(updates, !state.updatesManaged);
    y += kRow + 8;

    state.explanation = AddControl(state, L"STATIC", L"Select at least one metric to continue.",
                                   SS_LEFT | SS_NOPREFIX, kMargin, y, kColumn * 2, 20, kExplanation);
    y += 24;

    state.footerTop = y;
    const int buttonY = y + (kFooterHeight - kButtonHeight) / 2;
    const int cancelX = kClientWidth - kMargin - kButtonWidth;
    state.start = AddControl(state, L"BUTTON", state.firstRun ? L"&Start" : L"&Save", BS_DEFPUSHBUTTON | WS_TABSTOP,
                             cancelX - 8 - kButtonWidth, buttonY, kButtonWidth, kButtonHeight, IDOK);
    AddControl(state, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, cancelX, buttonY, kButtonWidth,
               kButtonHeight, IDCANCEL);
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
    if (!state.updatesManaged)
    {
        state.working.checkForUpdates = IsDlgButtonChecked(state.window, kCheckUpdates) == BST_CHECKED;
    }
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
        state->dpi = GetDpiForWindow(window);
        CreateFonts(*state);
        CreateControls(*state);
        ApplyLayout(*state);
        return 0;
    case WM_DPICHANGED: {
        state->dpi = HIWORD(wParam);
        CreateFonts(*state);
        ApplyLayout(*state);
        const auto *suggested = reinterpret_cast<const RECT *>(lParam);
        const auto size = WindowSize(*state);
        SetWindowPos(window, nullptr, suggested->left, suggested->top, size.cx, size.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        // Match each control's background to the surface it sits on.
        const auto id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
        const int surface = id == IDOK || id == IDCANCEL ? COLOR_BTNFACE : COLOR_WINDOW;
        auto *dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, GetSysColor(surface));
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(surface));
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        RECT footer{client.left, Scale(*state, state->footerTop), client.right, client.bottom};
        FillRect(dc, &footer, GetSysColorBrush(COLOR_BTNFACE));
        EndPaint(window, &paint);
        return 0;
    }
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
    state.updatesManaged = UpdateService::DisabledByPolicy();

    // Open on the monitor the user is working on (where the tray was clicked).
    POINT cursor{};
    GetCursorPos(&cursor);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor))
    {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &monitor.rcWork, 0);
    }

    if (owner)
    {
        EnableWindow(owner, FALSE);
    }
    HWND window = CreateWindowExW(kExStyle, kClassName, firstRun ? L"Welcome to Stat Wisp" : L"Stat Wisp settings",
                                  kStyle, monitor.rcWork.left, monitor.rcWork.top, 100, 100, owner, nullptr, instance,
                                  &state);
    if (!window)
    {
        if (owner)
        {
            EnableWindow(owner, TRUE);
        }
        return false;
    }
    const auto size = WindowSize(state);
    const auto workWidth = monitor.rcWork.right - monitor.rcWork.left;
    const auto workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
    const auto x = monitor.rcWork.left + (std::max)(0L, (workWidth - size.cx) / 2);
    const auto y = monitor.rcWork.top + (std::max)(0L, (workHeight - size.cy) / 2);
    SetWindowPos(window, HWND_TOP, x, y, size.cx, size.cy, SWP_NOACTIVATE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    SetFocus(state.metricControls[0]);

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
    // The owner is the hidden message window: re-enable it, but let Windows
    // return activation to the application the user was using before.
    if (owner)
    {
        EnableWindow(owner, TRUE);
    }
    if (state.accepted)
    {
        settings = state.working;
    }
    return state.accepted;
}

} // namespace statwisp
