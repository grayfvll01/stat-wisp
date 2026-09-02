#pragma once

#include "monitoring/MetricSnapshot.h"
#include "settings/Settings.h"

#include <chrono>
#include <memory>

namespace gate
{

class SensorFallbackProvider final
{
  public:
    SensorFallbackProvider();
    ~SensorFallbackProvider();
    SensorFallbackProvider(const SensorFallbackProvider &) = delete;
    SensorFallbackProvider &operator=(const SensorFallbackProvider &) = delete;

    void Collect(MetricSnapshot &snapshot, const Settings &settings);
    void Reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    MetricSnapshot cached_;
    std::chrono::steady_clock::time_point nextRead_{};
    std::chrono::steady_clock::time_point nextInitialize_{};
};

} // namespace gate
