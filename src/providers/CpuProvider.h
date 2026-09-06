#pragma once

#include "monitoring/MetricSnapshot.h"

#include <cstdint>

namespace statwisp
{

class CpuProvider final
{
  public:
    void Collect(MetricSnapshot &snapshot, bool usageNeeded, bool clockNeeded) noexcept;
    void Reset() noexcept;

  private:
    std::uint64_t previousIdle_{};
    std::uint64_t previousKernel_{};
    std::uint64_t previousUser_{};
    bool hasPrevious_{};
};

} // namespace statwisp
