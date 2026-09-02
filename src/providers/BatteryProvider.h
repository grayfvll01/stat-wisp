#pragma once

#include "monitoring/MetricSnapshot.h"

namespace gate
{

class BatteryProvider final
{
  public:
    static void Collect(MetricSnapshot &snapshot) noexcept;
};

} // namespace gate
