#pragma once

#include "providers/AmdProvider.h"
#include "providers/GpuPerformanceProvider.h"
#include "providers/NvidiaProvider.h"

#include <chrono>

namespace statwisp
{

class GpuProvider final
{
  public:
    void Collect(MetricSnapshot &snapshot, const Settings &settings);
    void Reset() noexcept;

  private:
    NvidiaProvider nvidia_;
    AmdProvider amd_;
    GpuPerformanceProvider performance_;
    unsigned int vendorFailures_{};
    unsigned int performanceFailures_{};
    std::chrono::steady_clock::time_point nextVendorRetry_{};
};

} // namespace statwisp
