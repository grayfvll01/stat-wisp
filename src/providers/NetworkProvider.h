#pragma once

#include "monitoring/MetricSnapshot.h"

#include <chrono>
#include <cstdint>

namespace statwisp
{

class NetworkProvider final
{
  public:
    void Collect(MetricSnapshot &snapshot, bool downloadNeeded, bool uploadNeeded) noexcept;
    void Reset() noexcept;

  private:
    std::uint64_t previousReceived_{};
    std::uint64_t previousSent_{};
    std::chrono::steady_clock::time_point previousTime_{};
    bool hasPrevious_{};
};

} // namespace statwisp
