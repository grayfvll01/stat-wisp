#include "providers/MemoryProvider.h"

#include <Windows.h>

namespace gate
{

void MemoryProvider::Collect(MetricSnapshot &snapshot) noexcept
{
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory))
    {
        snapshot.Set(MetricType::RamUsage, static_cast<double>(memory.dwMemoryLoad));
        snapshot.Set(MetricType::AvailableRam, static_cast<double>(memory.ullAvailPhys));
        if (memory.ullTotalPageFile != 0)
        {
            snapshot.Set(MetricType::CommitUsage,
                         100.0 * static_cast<double>(memory.ullTotalPageFile - memory.ullAvailPageFile) /
                             static_cast<double>(memory.ullTotalPageFile));
        }
    }
}

} // namespace gate
