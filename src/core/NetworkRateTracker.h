#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace statwisp
{

struct NetworkInterfaceCounters
{
    std::uint64_t identifier;
    std::uint64_t received;
    std::uint64_t sent;
};

struct NetworkRates
{
    double download{};
    double upload{};
};

class NetworkRateTracker final
{
  public:
    std::optional<NetworkRates> Sample(std::vector<NetworkInterfaceCounters> counters,
                                       std::chrono::steady_clock::time_point now)
    {
        std::optional<NetworkRates> rates;
        const auto seconds = std::chrono::duration<double>(now - previousTime_).count();
        if (hasPrevious_ && seconds > 0.0)
        {
            rates.emplace();
            for (const auto &current : counters)
            {
                const auto previous = std::find_if(previous_.begin(), previous_.end(), [&current](const auto &entry) {
                    return entry.identifier == current.identifier;
                });
                // A newly connected interface has no baseline. Its lifetime byte
                // count must not become traffic for the current sample.
                if (previous == previous_.end())
                {
                    continue;
                }
                if (current.received >= previous->received)
                {
                    rates->download += static_cast<double>(current.received - previous->received) / seconds;
                }
                if (current.sent >= previous->sent)
                {
                    rates->upload += static_cast<double>(current.sent - previous->sent) / seconds;
                }
            }
        }
        previous_ = std::move(counters);
        previousTime_ = now;
        hasPrevious_ = true;
        return rates;
    }

    void Reset() noexcept
    {
        previous_.clear();
        previousTime_ = {};
        hasPrevious_ = false;
    }

  private:
    std::vector<NetworkInterfaceCounters> previous_;
    std::chrono::steady_clock::time_point previousTime_{};
    bool hasPrevious_{};
};

} // namespace statwisp
