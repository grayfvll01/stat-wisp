#include "providers/CpuProvider.h"

#include <Windows.h>
#include <powrprof.h>

#include <algorithm>
#include <vector>

namespace statwisp
{
namespace
{

struct ProcessorPowerInformation
{
    ULONG number;
    ULONG maxMhz;
    ULONG currentMhz;
    ULONG mhzLimit;
    ULONG maxIdleState;
    ULONG currentIdleState;
};

std::uint64_t ToInteger(const FILETIME &value) noexcept
{
    ULARGE_INTEGER result{};
    result.LowPart = value.dwLowDateTime;
    result.HighPart = value.dwHighDateTime;
    return result.QuadPart;
}

} // namespace

void CpuProvider::Collect(MetricSnapshot &snapshot, bool usageNeeded, bool clockNeeded) noexcept
{
    if (usageNeeded)
    {
        FILETIME idle{}, kernel{}, user{};
        if (GetSystemTimes(&idle, &kernel, &user))
        {
            const auto currentIdle = ToInteger(idle);
            const auto currentKernel = ToInteger(kernel);
            const auto currentUser = ToInteger(user);
            if (hasPrevious_)
            {
                const auto idleDelta = currentIdle - previousIdle_;
                const auto kernelDelta = currentKernel - previousKernel_;
                const auto userDelta = currentUser - previousUser_;
                const auto total = kernelDelta + userDelta;
                if (total != 0 && total >= idleDelta)
                {
                    snapshot.Set(MetricType::CpuUsage,
                                 std::clamp(100.0 * static_cast<double>(total - idleDelta) / static_cast<double>(total),
                                            0.0, 100.0));
                }
            }
            previousIdle_ = currentIdle;
            previousKernel_ = currentKernel;
            previousUser_ = currentUser;
            hasPrevious_ = true;
        }
    }
    else
    {
        hasPrevious_ = false;
    }

    if (clockNeeded)
    {
        const auto processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (processorCount != 0)
        {
            std::vector<ProcessorPowerInformation> information(processorCount);
            const auto status =
                CallNtPowerInformation(ProcessorInformation, nullptr, 0, information.data(),
                                       static_cast<ULONG>(information.size() * sizeof(ProcessorPowerInformation)));
            if (status == 0)
            {
                std::uint64_t totalMHz = 0;
                for (const auto &processor : information)
                {
                    totalMHz += processor.currentMhz;
                }
                snapshot.Set(MetricType::CpuClock, static_cast<double>(totalMHz) / static_cast<double>(processorCount));
            }
        }
    }
}

void CpuProvider::Reset() noexcept
{
    previousIdle_ = previousKernel_ = previousUser_ = 0;
    hasPrevious_ = false;
}

} // namespace statwisp
