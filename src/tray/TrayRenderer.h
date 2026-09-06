#pragma once

#include "core/MetricFormatter.h"

#include <Windows.h>

#include <cstdint>
#include <optional>

namespace statwisp
{

class TrayRenderer final
{
  public:
    TrayRenderer() = default;
    ~TrayRenderer();
    TrayRenderer(const TrayRenderer &) = delete;
    TrayRenderer &operator=(const TrayRenderer &) = delete;

    [[nodiscard]] HICON Render(const FormattedMetric &metric, MetricType type, std::optional<double> value,
                               HWND window);
    void Invalidate() noexcept;
    [[nodiscard]] std::uint64_t Generation() const noexcept
    {
        return generation_;
    }

  private:
    void EnsureSurface(HWND window);
    void DestroySurface() noexcept;
    void RefreshTheme();

    HDC memoryDc_{};
    HBITMAP bitmap_{};
    HGDIOBJ oldBitmap_{};
    void *pixels_{};
    int size_{};
    UINT dpi_{};
    bool lightTaskbar_{};
    std::uint64_t generation_{1};
};

} // namespace statwisp
