#pragma once

#include "monitoring/MetricSnapshot.h"

namespace statwisp
{

class BatteryProvider final
{
  public:
    static void Collect(MetricSnapshot &snapshot) noexcept;
};

} // namespace statwisp
