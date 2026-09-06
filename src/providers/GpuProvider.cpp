#include "providers/GpuProvider.h"

namespace statwisp
{

void GpuProvider::Collect(MetricSnapshot &snapshot, const Settings &settings) noexcept
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
    if (vendorAvailable && vendorMetricRequested && !vendorValueReceived)
    {
        if (++vendorFailures_ >= 3)
        {
            nvidia_.Reset();
            amd_.Reset();
            vendorFailures_ = 0;
        }
    }
    else
    {
        vendorFailures_ = 0;
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
}

} // namespace statwisp
