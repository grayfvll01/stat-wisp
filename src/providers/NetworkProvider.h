#pragma once

#include "core/NetworkRateTracker.h"
#include "monitoring/MetricSnapshot.h"

namespace statwisp
{

class NetworkProvider final
{
  public:
    void Collect(MetricSnapshot &snapshot, bool downloadNeeded, bool uploadNeeded) noexcept;
    void Reset() noexcept;

  private:
    NetworkRateTracker rates_;
};

} // namespace statwisp
