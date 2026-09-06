#include "core/MetricFormatter.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace statwisp
{
namespace
{

std::wstring Number(double value, int precision)
{
    wchar_t buffer[32]{};
    std::swprintf(buffer, std::size(buffer), precision == 0 ? L"%.0f" : L"%.1f", value);
    return buffer;
}

std::wstring Percent(double value, const Settings &settings)
{
    value = std::clamp(value, 0.0, 100.0);
    return Number(value, settings.roundValues || value >= 99.95 ? 0 : 1);
}

std::wstring CompactRate(double value)
{
    value = std::max(0.0, value);
    static constexpr const wchar_t *kUnits[]{L"", L"K", L"M", L"G", L"T"};
    std::size_t unit = 0;
    while (value >= 999.5 && unit + 1 < std::size(kUnits))
    {
        value /= 1024.0;
        ++unit;
    }
    const auto precision = value < 10.0 && unit != 0 ? 1 : 0;
    return Number(value, precision) + kUnits[unit];
}

} // namespace

double MetricFormatter::CelsiusToFahrenheit(double celsius) noexcept
{
    return celsius * 9.0 / 5.0 + 32.0;
}

std::wstring MetricFormatter::FormatBytes(double bytes, bool perSecond, int precision)
{
    static constexpr const wchar_t *kUnits[]{L"B", L"K", L"M", L"G", L"T"};
    bytes = std::max(0.0, bytes);
    std::size_t unit = 0;
    while (bytes >= 1024.0 && unit + 1 < std::size(kUnits))
    {
        bytes /= 1024.0;
        ++unit;
    }
    const auto digits = bytes >= 100.0 || unit == 0 ? 0 : precision;
    auto result = Number(bytes, digits) + kUnits[unit];
    if (perSecond)
    {
        result += L"/s";
    }
    return result;
}

FormattedMetric MetricFormatter::Format(MetricType type, std::optional<double> value, const Settings &settings)
{
    FormattedMetric result;
    const auto &definition = Definition(type);
    if (!settings.compactLabels)
    {
        result.iconLabel.assign(definition.shortLabel);
    }
    if (!value || !std::isfinite(*value))
    {
        result.iconValue = L"--";
        result.tooltip = std::wstring(definition.name) + L": unavailable on this hardware/provider";
        if (type == MetricType::CpuTemperature)
        {
            result.tooltip = L"CPU Temperature: unavailable; run LibreHardwareMonitor with WMI enabled";
        }
        return result;
    }

    std::wstring longValue;
    switch (type)
    {
    case MetricType::CpuTemperature:
    case MetricType::GpuTemperature: {
        const auto temperature = settings.fahrenheit ? CelsiusToFahrenheit(*value) : *value;
        const auto iconPrecision = settings.roundValues || std::abs(temperature) >= 99.95 ? 0 : 1;
        result.iconValue = Number(temperature, iconPrecision);
        longValue = Number(temperature, settings.roundValues ? 0 : 1) +
                    (settings.fahrenheit ? L" °F" : L" °C");
        break;
    }
    case MetricType::CpuUsage:
    case MetricType::GpuUsage:
    case MetricType::RamUsage:
    case MetricType::CommitUsage:
    case MetricType::VramUsage:
    case MetricType::DiskUtilization:
    case MetricType::BatteryPercentage:
    case MetricType::GpuFanSpeed:
        result.iconValue = Percent(*value, settings);
        longValue = result.iconValue + L"%";
        break;
    case MetricType::CpuClock:
    case MetricType::GpuClock:
    case MetricType::GpuMemoryClock:
        if (*value >= 1000.0)
        {
            result.iconValue = Number(*value / 1000.0, 1) + L"G";
        }
        else
        {
            result.iconValue = Number(*value, 0);
        }
        longValue = Number(*value, 0) + L" MHz (reported)";
        break;
    case MetricType::NetworkDownload:
    case MetricType::NetworkUpload:
    case MetricType::DiskRead:
    case MetricType::DiskWrite:
        result.iconValue = CompactRate(*value);
        longValue = FormatBytes(*value, true, 1);
        break;
    case MetricType::AvailableRam:
        result.iconValue = CompactRate(*value);
        longValue = FormatBytes(*value, false, 1);
        break;
    case MetricType::CpuFanSpeed:
    case MetricType::SystemFanSpeed:
        result.iconValue = CompactRate(*value);
        longValue = Number(*value, 0) + L" RPM";
        break;
    case MetricType::Count:
        break;
    }
    result.tooltip = std::wstring(definition.name) + L": " + longValue;
    return result;
}

} // namespace statwisp
