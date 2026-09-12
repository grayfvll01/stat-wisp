#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace statwisp
{

class GpuEngineUsage final
{
  public:
    void Add(std::wstring_view instance, double usage)
    {
        // PDH GPU Engine instances describe one process on one physical engine:
        // pid_<pid>_luid_<high>_<low>_phys_<gpu>_eng_<engine>_engtype_<type>.
        const auto adapter = instance.find(L"_luid_");
        const auto physical = instance.find(L"_phys_", adapter);
        const auto engine = instance.find(L"_eng_", physical);
        const auto type = instance.find(L"_engtype_", engine);
        if (!instance.starts_with(L"pid_") || adapter == std::wstring_view::npos ||
            physical == std::wstring_view::npos || engine == std::wstring_view::npos ||
            type == std::wstring_view::npos || !std::isfinite(usage) || usage < 0.0)
        {
            return;
        }
        // Drop only the process and display type; preserve adapter, physical GPU,
        // and engine identity so independent engines are never added together.
        engines_[std::wstring(instance.substr(adapter, type - adapter))] += usage;
    }

    [[nodiscard]] std::optional<double> Busiest() const noexcept
    {
        std::optional<double> busiest;
        for (const auto &[identity, usage] : engines_)
        {
            if (!busiest || usage > *busiest)
            {
                busiest = std::clamp(usage, 0.0, 100.0);
            }
        }
        return busiest;
    }

  private:
    std::map<std::wstring, double> engines_;
};

} // namespace statwisp
