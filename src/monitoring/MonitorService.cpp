#include "monitoring/MonitorService.h"

#include <Objbase.h>

namespace statwisp
{

MonitorService::~MonitorService()
{
    Stop();
}

void MonitorService::Start(HWND notificationWindow, UINT notificationMessage, const Settings &settings)
{
    Stop();
    {
        std::scoped_lock lock(stateMutex_);
        notificationWindow_ = notificationWindow;
        notificationMessage_ = notificationMessage;
        settings_ = settings;
        paused_ = false;
        resetRequested_ = true;
        ++revision_;
    }
    worker_ = std::jthread([this](std::stop_token token) { Run(token); });
}

void MonitorService::Stop() noexcept
{
    if (worker_.joinable())
    {
        worker_.request_stop();
        wake_.notify_all();
        // Cancel a pending WMI/RPC call where supported. Never terminate just
        // the worker: that could leave a driver or CRT lock held in this process.
        CoCancelCall(GetThreadId(worker_.native_handle()), 0);
        if (WaitForSingleObject(worker_.native_handle(), 3000) != WAIT_OBJECT_0)
        {
            // A third-party provider can ignore cancellation. All settings are
            // already saved; Windows reclaims this process's resources on exit.
            TerminateProcess(GetCurrentProcess(), ERROR_TIMEOUT);
        }
        worker_.join();
    }
}

void MonitorService::UpdateSettings(const Settings &settings)
{
    {
        std::scoped_lock lock(stateMutex_);
        settings_ = settings;
        resetRequested_ = true;
        ++revision_;
    }
    wake_.notify_all();
}

void MonitorService::SetPaused(bool paused)
{
    {
        std::scoped_lock lock(stateMutex_);
        paused_ = paused;
        ++revision_;
    }
    wake_.notify_all();
}

void MonitorService::RestartProviders()
{
    {
        std::scoped_lock lock(stateMutex_);
        resetRequested_ = true;
        ++revision_;
    }
    wake_.notify_all();
}

MetricSnapshot MonitorService::LatestSnapshot() const
{
    std::scoped_lock lock(snapshotMutex_);
    return latest_;
}

void MonitorService::ResetProviders()
{
    cpu_.Reset();
    sensors_.Reset();
    network_.Reset();
    disk_.Reset();
    gpu_.Reset();
}

void MonitorService::Run(std::stop_token stopToken)
{
    const auto comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(comResult))
    {
        CoEnableCallCancellation(nullptr);
        CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                             nullptr, EOAC_NONE, nullptr);
    }

    while (!stopToken.stop_requested())
    {
        Settings current;
        bool paused = false;
        bool shouldReset = false;
        std::uint64_t observedRevision = 0;
        {
            std::scoped_lock lock(stateMutex_);
            current = settings_;
            paused = paused_;
            shouldReset = resetRequested_;
            resetRequested_ = false;
            observedRevision = revision_;
        }

        if (shouldReset)
        {
            ResetProviders();
        }

        if (!paused)
        {
#ifdef STAT_WISP_TEST_STALLED_PROVIDER
            Sleep(INFINITE); // Only compiled into the shutdown regression test.
#endif
            MetricSnapshot snapshot;
            const auto providers = current.RequiredProviders();

            if (HasProvider(providers, Provider::CpuNative))
            {
                cpu_.Collect(snapshot, current.IsEnabled(MetricType::CpuUsage),
                             current.IsEnabled(MetricType::CpuClock));
            }
            if (HasProvider(providers, Provider::Memory))
            {
                memory_.Collect(snapshot);
            }
            if (HasProvider(providers, Provider::Gpu))
            {
                gpu_.Collect(snapshot, current);
            }
            const bool sensorFallbackNeeded = HasProvider(providers, Provider::Sensors) ||
                                              (current.IsEnabled(MetricType::GpuTemperature) &&
                                               !snapshot.Get(MetricType::GpuTemperature));
            if (sensorFallbackNeeded)
            {
                sensors_.Collect(snapshot, current);
            }
            if (HasProvider(providers, Provider::Network))
            {
                network_.Collect(snapshot, current.IsEnabled(MetricType::NetworkDownload),
                                 current.IsEnabled(MetricType::NetworkUpload));
            }
            if (HasProvider(providers, Provider::Disk))
            {
                disk_.Collect(snapshot, current.IsEnabled(MetricType::DiskRead),
                              current.IsEnabled(MetricType::DiskWrite), current.IsEnabled(MetricType::DiskUtilization));
            }
            if (HasProvider(providers, Provider::Battery))
            {
                BatteryProvider::Collect(snapshot);
            }
            snapshot.sampledAt = std::chrono::steady_clock::now();
            {
                std::scoped_lock lock(snapshotMutex_);
                latest_ = snapshot;
            }
            if (notificationWindow_ && notificationMessage_)
            {
                PostMessageW(notificationWindow_, notificationMessage_, 0, 0);
            }
        }

        std::unique_lock lock(stateMutex_);
        if (paused_)
        {
            wake_.wait(lock, stopToken, [this, observedRevision] { return revision_ != observedRevision; });
        }
        else
        {
            wake_.wait_for(lock, stopToken, std::chrono::milliseconds(current.updateIntervalMs),
                           [this, observedRevision] { return revision_ != observedRevision; });
        }
    }

    ResetProviders();
    if (SUCCEEDED(comResult))
    {
        CoDisableCallCancellation(nullptr);
        CoUninitialize();
    }
}

} // namespace statwisp
