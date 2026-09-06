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

    void Sync(const Settings &settings, const MetricSnapshot &snapshot);
    void Refresh(const Settings &settings, const MetricSnapshot &snapshot);
    void Recreate(const Settings &settings, const MetricSnapshot &snapshot);
    void RemoveAll() noexcept;

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
