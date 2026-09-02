#include "providers/BatteryProvider.h"

#include <Windows.h>

namespace gate
{

void BatteryProvider::Collect(MetricSnapshot &snapshot) noexcept
{
    SYSTEM_POWER_STATUS status{};
    if (GetSystemPowerStatus(&status) && status.BatteryFlag != 128 && status.BatteryLifePercent <= 100)
    {
        snapshot.Set(MetricType::BatteryPercentage, static_cast<double>(status.BatteryLifePercent));
    }
}

} // namespace gate
