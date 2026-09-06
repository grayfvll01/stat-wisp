#pragma once

#include "monitoring/MetricSnapshot.h"

#include <chrono>
#include <memory>

namespace statwisp
{

class DiskProvider final
{
  public:
    DiskProvider();
    ~DiskProvider();
    DiskProvider(const DiskProvider &) = delete;
    DiskProvider &operator=(const DiskProvider &) = delete;

    void Collect(MetricSnapshot &snapshot, bool readNeeded, bool writeNeeded, bool utilizationNeeded) noexcept;
    void Reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::chrono::steady_clock::time_point nextInitialize_{};
};

} // namespace statwisp
