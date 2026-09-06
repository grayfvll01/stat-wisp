#pragma once

#include <chrono>
#include <memory>
#include <optional>

namespace statwisp
{

class GpuPerformanceProvider final
{
  public:
    GpuPerformanceProvider();
    ~GpuPerformanceProvider();
    GpuPerformanceProvider(const GpuPerformanceProvider &) = delete;
    GpuPerformanceProvider &operator=(const GpuPerformanceProvider &) = delete;

    [[nodiscard]] std::optional<double> ReadUsage() noexcept;
    void Reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::chrono::steady_clock::time_point nextInitialize_{};
};

} // namespace statwisp
