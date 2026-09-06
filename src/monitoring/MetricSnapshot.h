#pragma once

#include "monitoring/Metric.h"

#include <array>
#include <chrono>
#include <optional>

namespace statwisp
{

struct MetricSnapshot
{
    std::array<std::optional<double>, kMetricCount> values{};
    std::chrono::steady_clock::time_point sampledAt{};

    void Set(MetricType type, double value) noexcept
    {
        values[MetricIndex(type)] = value;
    }
    [[nodiscard]] std::optional<double> Get(MetricType type) const noexcept
    {
        return values[MetricIndex(type)];
    }
};

} // namespace statwisp
