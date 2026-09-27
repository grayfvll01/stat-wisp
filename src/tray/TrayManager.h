#pragma once

#include "monitoring/MetricSnapshot.h"
#include "settings/Settings.h"
#include "tray/TrayRenderer.h"

#include <Windows.h>

#include <array>
#include <string>

namespace statwisp
{

inline constexpr UINT kTrayCallbackMessage = WM_APP + 1;

class TrayManager final
{
  public:
    TrayManager(HWND window, TrayRenderer &renderer);
    ~TrayManager();
    TrayManager(const TrayManager &) = delete;
    TrayManager &operator=(const TrayManager &) = delete;

    // Each returns false when an enabled icon could not be shown (for example
    // while Explorer is still starting); calling again retries it.
    bool Sync(const Settings &settings, const MetricSnapshot &snapshot);
    bool Refresh(const Settings &settings, const MetricSnapshot &snapshot);
    bool Recreate(const Settings &settings, const MetricSnapshot &snapshot);
    void RemoveAll() noexcept;
    // Shows a notification from the first visible icon. Returns false if none is visible.
    bool ShowBalloon(const std::wstring &title, const std::wstring &text) noexcept;

  private:
    struct Entry
    {
        bool added{};
        HICON icon{};
        std::wstring renderKey;
    };

    bool AddOrUpdate(MetricType type, Entry &entry, const FormattedMetric &formatted, std::optional<double> value,
                     bool force);
    void Remove(MetricType type, Entry &entry) noexcept;

    HWND window_{};
    TrayRenderer &renderer_;
    std::array<Entry, kMetricCount> entries_{};
};

} // namespace statwisp
