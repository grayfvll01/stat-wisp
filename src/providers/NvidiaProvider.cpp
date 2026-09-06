#include "providers/NvidiaProvider.h"

#include <array>

namespace statwisp
{
namespace
{

template <typename Function> Function Resolve(HMODULE library, const char *name) noexcept
{
    return reinterpret_cast<Function>(GetProcAddress(library, name));
}

} // namespace

NvidiaProvider::~NvidiaProvider()
{
    Reset();
}

bool NvidiaProvider::Initialize() noexcept
{
    if (Available())
    {
        return true;
    }
    if (attempted_)
    {
        return false;
    }
    attempted_ = true;

    library_ = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library_)
    {
        std::array<wchar_t, 1024> expanded{};
        const auto length = ExpandEnvironmentStringsW(L"%ProgramW6432%\\NVIDIA Corporation\\NVSMI\\nvml.dll", expanded.data(),
                                                       static_cast<DWORD>(expanded.size()));
        if (length > 0 && length <= expanded.size())
        {
            library_ = LoadLibraryExW(expanded.data(), nullptr,
                                      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        }
    }
    if (!library_)
    {
        return false;
    }

    init_ = Resolve<InitFn>(library_, "nvmlInit_v2");
    shutdown_ = Resolve<ShutdownFn>(library_, "nvmlShutdown");
    auto getCount = Resolve<GetCountFn>(library_, "nvmlDeviceGetCount_v2");
    auto getHandle = Resolve<GetHandleFn>(library_, "nvmlDeviceGetHandleByIndex_v2");
    getTemperature_ = Resolve<GetTemperatureFn>(library_, "nvmlDeviceGetTemperature");
    getFanSpeed_ = Resolve<GetFanSpeedFn>(library_, "nvmlDeviceGetFanSpeed");
    getUtilization_ = Resolve<GetUtilizationFn>(library_, "nvmlDeviceGetUtilizationRates");
    getClock_ = Resolve<GetClockFn>(library_, "nvmlDeviceGetClockInfo");
    getMemory_ = Resolve<GetMemoryFn>(library_, "nvmlDeviceGetMemoryInfo");
    if (!init_ || !shutdown_ || !getCount || !getHandle || init_() != 0)
    {
        Reset();
        attempted_ = true;
        return false;
    }
    initialized_ = true;

    unsigned int count = 0;
    if (getCount(&count) != 0 || count == 0 || getHandle(0, &device_) != 0)
    {
        Reset();
        attempted_ = true;
        return false;
    }
    return true;
}

void NvidiaProvider::Collect(MetricSnapshot &snapshot, const Settings &settings) noexcept
{
    if (!Available())
    {
        return;
    }
    if (settings.IsEnabled(MetricType::GpuTemperature) && getTemperature_)
    {
        unsigned int value = 0;
        if (getTemperature_(device_, 0, &value) == 0 && value <= 125)
        {
            snapshot.Set(MetricType::GpuTemperature, static_cast<double>(value));
        }
    }
    if (settings.IsEnabled(MetricType::GpuUsage) && getUtilization_)
    {
        Utilization value{};
        if (getUtilization_(device_, &value) == 0)
        {
            snapshot.Set(MetricType::GpuUsage, static_cast<double>(value.gpu));
        }
    }
    if (settings.IsEnabled(MetricType::GpuFanSpeed) && getFanSpeed_)
    {
        unsigned int value = 0;
        if (getFanSpeed_(device_, &value) == 0 && value <= 100)
        {
            snapshot.Set(MetricType::GpuFanSpeed, static_cast<double>(value));
        }
    }
    if (settings.IsEnabled(MetricType::GpuClock) && getClock_)
    {
        unsigned int value = 0;
        if (getClock_(device_, 0, &value) == 0)
        {
            snapshot.Set(MetricType::GpuClock, static_cast<double>(value));
        }
    }
    if (settings.IsEnabled(MetricType::GpuMemoryClock) && getClock_)
    {
        unsigned int value = 0;
        if (getClock_(device_, 2, &value) == 0)
        {
            snapshot.Set(MetricType::GpuMemoryClock, static_cast<double>(value));
        }
    }
    if (settings.IsEnabled(MetricType::VramUsage) && getMemory_)
    {
        Memory value{};
        if (getMemory_(device_, &value) == 0 && value.total != 0)
        {
            snapshot.Set(MetricType::VramUsage,
                         100.0 * static_cast<double>(value.used) / static_cast<double>(value.total));
        }
    }
}

void NvidiaProvider::Reset() noexcept
{
    device_ = nullptr;
    if (initialized_ && shutdown_)
    {
        shutdown_();
    }
    initialized_ = false;
    init_ = nullptr;
    shutdown_ = nullptr;
    getTemperature_ = nullptr;
    getFanSpeed_ = nullptr;
    getUtilization_ = nullptr;
    getClock_ = nullptr;
    getMemory_ = nullptr;
    if (library_)
    {
        FreeLibrary(library_);
        library_ = nullptr;
    }
    attempted_ = false;
}

} // namespace statwisp
