#pragma once

#include "providers/AmdProvider.h"
#include "providers/GpuPerformanceProvider.h"
#include "providers/NvidiaProvider.h"

namespace statwisp
{

class GpuProvider final
{
  public:
    void Collect(MetricSnapshot &snapshot, const Settings &settings) noexcept;
    void Reset() noexcept;

  private:
    NvidiaProvider nvidia_;
    AmdProvider amd_;
    GpuPerformanceProvider performance_;
    unsigned int vendorFailures_{};
    unsigned int performanceFailures_{};
};

} // namespace statwisp
