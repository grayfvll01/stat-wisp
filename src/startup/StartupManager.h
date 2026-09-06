#pragma once

namespace statwisp
{

class StartupManager final
{
  public:
    [[nodiscard]] static bool SetEnabled(bool enabled) noexcept;
    [[nodiscard]] static bool IsEnabled() noexcept;
};

} // namespace statwisp
