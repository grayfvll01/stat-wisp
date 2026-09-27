#include "tray/TrayManager.h"

#include "core/MetricFormatter.h"

#include <shellapi.h>
#include <strsafe.h>

namespace statwisp
{
namespace
{

UINT IconId(MetricType type) noexcept
{
    return 1000U + static_cast<UINT>(MetricIndex(type));
}

} // namespace

TrayManager::TrayManager(HWND window, TrayRenderer &renderer) : window_(window), renderer_(renderer)
{
}

TrayManager::~TrayManager()
{
    RemoveAll();
}

bool TrayManager::Sync(const Settings &settings, const MetricSnapshot &snapshot)
{
    bool complete = true;
    for (const auto type : settings.order)
    {
        auto &entry = entries_[MetricIndex(type)];
        if (!settings.IsEnabled(type))
        {
            Remove(type, entry);
            continue;
        }
        const auto value = snapshot.Get(type);
        const auto formatted = MetricFormatter::Format(type, value, settings);
        complete = AddOrUpdate(type, entry, formatted, value, false) && complete;
    }
    return complete;
}

bool TrayManager::Refresh(const Settings &settings, const MetricSnapshot &snapshot)
{
    renderer_.Invalidate();
    for (auto &entry : entries_)
    {
        entry.renderKey.clear();
    }
    return Sync(settings, snapshot);
}

bool TrayManager::Recreate(const Settings &settings, const MetricSnapshot &snapshot)
{
    for (auto &entry : entries_)
    {
        entry.added = false;
        entry.renderKey.clear();
    }
    return Sync(settings, snapshot);
}

bool TrayManager::AddOrUpdate(MetricType type, Entry &entry, const FormattedMetric &formatted,
                              std::optional<double> value, bool force)
{
    std::wstring temperatureBand;
    if ((type == MetricType::CpuTemperature || type == MetricType::GpuTemperature) && value)
    {
        temperatureBand = *value < 60.0 ? L"cool" : (*value < 80.0 ? L"warm" : L"hot");
    }
    const auto key = formatted.iconLabel + L"|" + formatted.iconValue + L"|" + formatted.tooltip + L"|" +
                     temperatureBand + L"|" + std::to_wstring(renderer_.Generation());
    if (entry.added && !force && entry.renderKey == key)
    {
        return true;
    }

    HICON replacement = renderer_.Render(formatted, type, value, window_);
    if (!replacement)
    {
        return false;
    }
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = IconId(type);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = kTrayCallbackMessage;
    data.hIcon = replacement;
    StringCchCopyW(data.szTip, std::size(data.szTip), formatted.tooltip.c_str());

    bool added = !entry.added && Shell_NotifyIconW(NIM_ADD, &data);
    // NIM_ADD can report a timeout although Explorer added the icon, and an
    // icon can disappear before TaskbarCreated arrives. Try the other operation.
    bool shown = added || Shell_NotifyIconW(NIM_MODIFY, &data);
    if (!shown && entry.added)
    {
        added = shown = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    }
    if (!shown)
    {
        DestroyIcon(replacement);
        entry.added = false;
        entry.renderKey.clear();
        return false;
    }
    if (added || !entry.added)
    {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
    if (entry.icon)
    {
        DestroyIcon(entry.icon);
    }
    entry.icon = replacement;
    entry.added = true;
    entry.renderKey = key;
    return true;
}

void TrayManager::Remove(MetricType type, Entry &entry) noexcept
{
    if (entry.added)
    {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window_;
        data.uID = IconId(type);
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    if (entry.icon)
    {
        DestroyIcon(entry.icon);
    }
    entry = {};
}

bool TrayManager::ShowBalloon(const std::wstring &title, const std::wstring &text) noexcept
{
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        if (!entries_[index].added)
        {
            continue;
        }
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window_;
        data.uID = IconId(static_cast<MetricType>(index));
        data.uFlags = NIF_INFO;
        data.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
        StringCchCopyW(data.szInfoTitle, std::size(data.szInfoTitle), title.c_str());
        StringCchCopyW(data.szInfo, std::size(data.szInfo), text.c_str());
        return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
    }
    return false;
}

void TrayManager::RemoveAll() noexcept
{
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        Remove(static_cast<MetricType>(index), entries_[index]);
    }
}

} // namespace statwisp
