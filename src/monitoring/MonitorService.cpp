#include "monitoring/MonitorService.h"

#include "core/Log.h"

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
    failureLogged_.fill(false);
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

            // One failing provider (for example an allocation failure or a
            // misbehaving driver library) must not stop the others or the thread.
            const auto guarded = [this](std::size_t index, const wchar_t *failure, auto &&collect, auto &&reset) {
                try
                {
                    collect();
                }
                catch (...)
                {
                    reset();
                    if (!failureLogged_[index])
                    {
                        failureLogged_[index] = true;
                        LogError(failure);
                    }
                }
            };

            if (HasProvider(providers, Provider::CpuNative))
            {
                guarded(
                    0, L"CPU readings failed and were reset.",
                    [&] {
                        cpu_.Collect(snapshot, current.IsEnabled(MetricType::CpuUsage),
                                     current.IsEnabled(MetricType::CpuClock));
                    },
                    [this] { cpu_.Reset(); });
            }
            if (HasProvider(providers, Provider::Memory))
            {
                guarded(1, L"Memory readings failed.", [&] { memory_.Collect(snapshot); }, [] {});
            }
            if (HasProvider(providers, Provider::Gpu))
            {
                guarded(2, L"GPU readings failed and were reset.", [&] { gpu_.Collect(snapshot, current); },
                        [this] { gpu_.Reset(); });
            }
            const bool sensorFallbackNeeded = HasProvider(providers, Provider::Sensors) ||
                                              (current.IsEnabled(MetricType::GpuTemperature) &&
                                               !snapshot.Get(MetricType::GpuTemperature));
            if (sensorFallbackNeeded)
            {
                guarded(3, L"External sensor readings failed and were reset.",
                        [&] { sensors_.Collect(snapshot, current); }, [this] { sensors_.Reset(); });
            }
            if (HasProvider(providers, Provider::Network))
            {
                guarded(
                    4, L"Network readings failed and were reset.",
                    [&] {
                        network_.Collect(snapshot, current.IsEnabled(MetricType::NetworkDownload),
                                         current.IsEnabled(MetricType::NetworkUpload));
                    },
                    [this] { network_.Reset(); });
            }
            if (HasProvider(providers, Provider::Disk))
            {
                guarded(
                    5, L"Disk readings failed and were reset.",
                    [&] {
                        disk_.Collect(snapshot, current.IsEnabled(MetricType::DiskRead),
                                      current.IsEnabled(MetricType::DiskWrite),
                                      current.IsEnabled(MetricType::DiskUtilization));
                    },
                    [this] { disk_.Reset(); });
            }
            if (HasProvider(providers, Provider::Battery))
            {
                guarded(6, L"Battery readings failed.", [&] { BatteryProvider::Collect(snapshot); }, [] {});
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
