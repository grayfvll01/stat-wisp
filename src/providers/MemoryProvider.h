#pragma once

#include "monitoring/MetricSnapshot.h"

namespace gate
{

class MemoryProvider final
{
  public:
    static void Collect(MetricSnapshot &snapshot) noexcept;
};

} // namespace gate
