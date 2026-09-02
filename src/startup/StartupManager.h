#pragma once

namespace gate
{

class StartupManager final
{
  public:
    [[nodiscard]] static bool SetEnabled(bool enabled) noexcept;
    [[nodiscard]] static bool IsEnabled() noexcept;
};

} // namespace gate
