#pragma once

#include "monitoring/MetricSnapshot.h"
#include "providers/CpuProvider.h"
#include "providers/SensorFallbackProvider.h"
#include "providers/DiskProvider.h"
#include "providers/GpuProvider.h"
#include "providers/BatteryProvider.h"
#include "providers/MemoryProvider.h"
#include "providers/NetworkProvider.h"
#include "settings/Settings.h"

#include <Windows.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace statwisp
{

class MonitorService final
{
  public:
    MonitorService() = default;
    ~MonitorService();
    MonitorService(const MonitorService &) = delete;
    MonitorService &operator=(const MonitorService &) = delete;

    void Start(HWND notificationWindow, UINT notificationMessage, const Settings &settings);
    void Stop() noexcept;
    void UpdateSettings(const Settings &settings);
    void SetPaused(bool paused);
    void RestartProviders();
    [[nodiscard]] MetricSnapshot LatestSnapshot() const;

  private:
    void Run(std::stop_token stopToken);
    void ResetProviders();

    mutable std::mutex stateMutex_;
    std::condition_variable_any wake_;
    Settings settings_;
    bool paused_{};
    bool resetRequested_{};
    std::uint64_t revision_{};
    HWND notificationWindow_{};
    UINT notificationMessage_{};
    std::jthread worker_;

    mutable std::mutex snapshotMutex_;
    MetricSnapshot latest_;

    CpuProvider cpu_;
    SensorFallbackProvider sensors_;
    MemoryProvider memory_;
    NetworkProvider network_;
    DiskProvider disk_;
    GpuProvider gpu_;
};

} // namespace statwisp
