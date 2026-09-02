#pragma once

#include "monitoring/Metric.h"
#include "settings/Settings.h"

#include <optional>
#include <string>

namespace gate
{

struct FormattedMetric
{
    std::wstring iconLabel;
    std::wstring iconValue;
    std::wstring tooltip;
};

class MetricFormatter final
{
  public:
    [[nodiscard]] static FormattedMetric Format(MetricType type, std::optional<double> value, const Settings &settings);
    [[nodiscard]] static std::wstring FormatBytes(double bytes, bool perSecond = false, int precision = 1);
    [[nodiscard]] static double CelsiusToFahrenheit(double celsius) noexcept;
};

} // namespace gate
