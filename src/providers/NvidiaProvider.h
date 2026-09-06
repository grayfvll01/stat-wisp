#pragma once

#include "monitoring/MetricSnapshot.h"
#include "settings/Settings.h"

#include <Windows.h>

namespace statwisp
{

class NvidiaProvider final
{
  public:
    NvidiaProvider() = default;
    ~NvidiaProvider();
    NvidiaProvider(const NvidiaProvider &) = delete;
    NvidiaProvider &operator=(const NvidiaProvider &) = delete;

    [[nodiscard]] bool Initialize() noexcept;
    [[nodiscard]] bool Available() const noexcept
    {
        return device_ != nullptr;
    }
    void Collect(MetricSnapshot &snapshot, const Settings &settings) noexcept;
    void Reset() noexcept;

  private:
    using Return = int;
    using Device = void *;
    using InitFn = Return (*)();
    using ShutdownFn = Return (*)();
    using GetCountFn = Return (*)(unsigned int *);
    using GetHandleFn = Return (*)(unsigned int, Device *);
    using GetTemperatureFn = Return (*)(Device, unsigned int, unsigned int *);
    using GetFanSpeedFn = Return (*)(Device, unsigned int *);
    struct Utilization
    {
        unsigned int gpu;
        unsigned int memory;
    };
    using GetUtilizationFn = Return (*)(Device, Utilization *);
    using GetClockFn = Return (*)(Device, unsigned int, unsigned int *);
    struct Memory
    {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };
    using GetMemoryFn = Return (*)(Device, Memory *);

    HMODULE library_{};
    Device device_{};
    InitFn init_{};
    ShutdownFn shutdown_{};
    GetTemperatureFn getTemperature_{};
    GetFanSpeedFn getFanSpeed_{};
    GetUtilizationFn getUtilization_{};
    GetClockFn getClock_{};
    GetMemoryFn getMemory_{};
    bool initialized_{};
    bool attempted_{};
};

} // namespace statwisp
