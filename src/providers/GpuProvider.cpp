#include "providers/GpuProvider.h"

namespace statwisp
{

void GpuProvider::Collect(MetricSnapshot &snapshot, const Settings &settings)
{
    bool vendorAvailable = false;
    bool vendorMetricRequested = false;
    if (nvidia_.Initialize())
    {
        vendorAvailable = true;
        vendorMetricRequested = settings.IsEnabled(MetricType::GpuTemperature) ||
                                settings.IsEnabled(MetricType::GpuUsage) || settings.IsEnabled(MetricType::GpuClock) ||
                                settings.IsEnabled(MetricType::GpuMemoryClock) ||
                                settings.IsEnabled(MetricType::VramUsage) ||
                                settings.IsEnabled(MetricType::GpuFanSpeed);
        nvidia_.Collect(snapshot, settings);
    }
    else if (amd_.Initialize())
    {
        vendorAvailable = true;
        vendorMetricRequested = settings.IsEnabled(MetricType::GpuTemperature) ||
                                settings.IsEnabled(MetricType::GpuUsage) || settings.IsEnabled(MetricType::GpuClock) ||
                                settings.IsEnabled(MetricType::GpuMemoryClock) ||
                                settings.IsEnabled(MetricType::GpuFanSpeed);
        amd_.Collect(snapshot, settings);
    }

    const bool vendorValueReceived =
        snapshot.Get(MetricType::GpuTemperature).has_value() || snapshot.Get(MetricType::GpuUsage).has_value() ||
        snapshot.Get(MetricType::GpuClock).has_value() || snapshot.Get(MetricType::GpuMemoryClock).has_value() ||
        snapshot.Get(MetricType::VramUsage).has_value() || snapshot.Get(MetricType::GpuFanSpeed).has_value();
    // Reload a vendor library at most once a minute: often enough to recover
    // from a driver that finished loading after logon or was restarted, but
    // never in a loop for a metric the device simply does not report.
    const auto now = std::chrono::steady_clock::now();
    const bool vendorMissing =
        !vendorAvailable && (settings.IsEnabled(MetricType::GpuTemperature) ||
                             settings.IsEnabled(MetricType::GpuClock) || settings.IsEnabled(MetricType::GpuMemoryClock) ||
                             settings.IsEnabled(MetricType::VramUsage) || settings.IsEnabled(MetricType::GpuFanSpeed));
    const bool vendorSilent = vendorAvailable && vendorMetricRequested && !vendorValueReceived;
    vendorFailures_ = vendorSilent ? vendorFailures_ + 1 : 0;
    if ((vendorMissing || vendorFailures_ >= 3) && now >= nextVendorRetry_)
    {
        if (nextVendorRetry_ != std::chrono::steady_clock::time_point{})
        {
            nvidia_.Reset();
            amd_.Reset();
            vendorFailures_ = 0;
        }
        nextVendorRetry_ = now + std::chrono::minutes(1);
    }

    if (settings.IsEnabled(MetricType::GpuUsage) && !snapshot.Get(MetricType::GpuUsage))
    {
        if (const auto usage = performance_.ReadUsage())
        {
            snapshot.Set(MetricType::GpuUsage, *usage);
            performanceFailures_ = 0;
        }
        else if (++performanceFailures_ >= 3)
        {
            performance_.Reset();
            performanceFailures_ = 0;
        }
    }
    else
    {
        performanceFailures_ = 0;
    }
}

void GpuProvider::Reset() noexcept
{
    nvidia_.Reset();
    amd_.Reset();
    performance_.Reset();
    vendorFailures_ = 0;
    performanceFailures_ = 0;
    nextVendorRetry_ = {};
}

} // namespace statwisp
