#include "providers/AmdProvider.h"

#include <cstdlib>
#include <vector>

namespace statwisp
{
namespace
{

constexpr int kAdlOk = 0;
constexpr int kAmdVendorId = 0x1002;
constexpr std::size_t kAdlPath = 256;

struct AdapterInfo
{
    int size;
    int adapterIndex;
    char udid[kAdlPath];
    int busNumber;
    int deviceNumber;
    int functionNumber;
    int vendorId;
    char adapterName[kAdlPath];
    char displayName[kAdlPath];
    int present;
    int exists;
    char driverPath[kAdlPath];
    char driverPathExt[kAdlPath];
    char pnpString[kAdlPath];
    int osDisplayIndex;
};

struct Activity
{
    int size;
    int engineClock;
    int memoryClock;
    int voltage;
    int activityPercent;
    int currentPerformanceLevel;
    int currentBusSpeed;
    int currentBusLanes;
    int maximumBusLanes;
    int reserved;
};

struct Temperature
{
    int size;
    int milliCelsius;
};

struct FanSpeed
{
    int size;
    int speedType;
    int fanSpeed;
    int flags;
};

constexpr int kFanSpeedPercent = 1;

void *__stdcall AdlAllocate(int bytes)
{
    return bytes > 0 ? std::malloc(static_cast<std::size_t>(bytes)) : nullptr;
}

template <typename Function> Function Resolve(HMODULE library, const char *name) noexcept
{
    return reinterpret_cast<Function>(GetProcAddress(library, name));
}

} // namespace

AmdProvider::~AmdProvider()
{
    Reset();
}

bool AmdProvider::Initialize() noexcept
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
    library_ =
        LoadLibraryExW(L"atiadlxx.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library_)
    {
        return false;
    }

    const auto create = Resolve<CreateFn>(library_, "ADL2_Main_Control_Create");
    destroy_ = Resolve<DestroyFn>(library_, "ADL2_Main_Control_Destroy");
    const auto countAdapters = Resolve<CountFn>(library_, "ADL2_Adapter_NumberOfAdapters_Get");
    const auto getInfo = Resolve<InfoFn>(library_, "ADL2_Adapter_AdapterInfo_Get");
    const auto caps = Resolve<CapsFn>(library_, "ADL2_Overdrive_Caps");
    activity_ = Resolve<ActivityFn>(library_, "ADL2_Overdrive5_CurrentActivity_Get");
    temperature_ = Resolve<TemperatureFn>(library_, "ADL2_Overdrive5_Temperature_Get");
    fanSpeed_ = Resolve<FanSpeedFn>(library_, "ADL2_Overdrive5_FanSpeed_Get");
    if (!create || !destroy_ || !countAdapters || !getInfo || !caps || create(AdlAllocate, 1, &context_) != kAdlOk)
    {
        Reset();
        attempted_ = true;
        return false;
    }

    int count = 0;
    if (countAdapters(context_, &count) != kAdlOk || count <= 0 || count > 256)
    {
        Reset();
        attempted_ = true;
        return false;
    }
    std::vector<AdapterInfo> adapters(static_cast<std::size_t>(count));
    for (auto &adapter : adapters)
    {
        adapter.size = sizeof(AdapterInfo);
    }
    if (getInfo(context_, adapters.data(), static_cast<int>(adapters.size() * sizeof(AdapterInfo))) != kAdlOk)
    {
        Reset();
        attempted_ = true;
        return false;
    }
    for (const auto &adapter : adapters)
    {
        int supported = 0;
        int enabled = 0;
        int version = 0;
        if (adapter.vendorId == kAmdVendorId && adapter.present &&
            caps(context_, adapter.adapterIndex, &supported, &enabled, &version) == kAdlOk && supported && version >= 5)
        {
            adapterIndex_ = adapter.adapterIndex;
            break;
        }
    }
    if (adapterIndex_ < 0)
    {
        Reset();
        attempted_ = true;
        return false;
    }
    return true;
}

void AmdProvider::Collect(MetricSnapshot &snapshot, const Settings &settings) noexcept
{
    if (!Available())
    {
        return;
    }
    if (settings.IsEnabled(MetricType::GpuTemperature) && temperature_)
    {
        Temperature value{sizeof(Temperature), 0};
        if (temperature_(context_, adapterIndex_, 0, &value) == kAdlOk && value.milliCelsius > 0)
        {
            snapshot.Set(MetricType::GpuTemperature, static_cast<double>(value.milliCelsius) / 1000.0);
        }
    }
    if ((settings.IsEnabled(MetricType::GpuUsage) || settings.IsEnabled(MetricType::GpuClock) ||
         settings.IsEnabled(MetricType::GpuMemoryClock)) &&
        activity_)
    {
        Activity value{};
        value.size = sizeof(Activity);
        if (activity_(context_, adapterIndex_, &value) == kAdlOk)
        {
            if (settings.IsEnabled(MetricType::GpuUsage))
            {
                snapshot.Set(MetricType::GpuUsage, static_cast<double>(value.activityPercent));
            }
            if (settings.IsEnabled(MetricType::GpuClock) && value.engineClock > 0)
            {
                snapshot.Set(MetricType::GpuClock, static_cast<double>(value.engineClock) / 100.0);
            }
            if (settings.IsEnabled(MetricType::GpuMemoryClock) && value.memoryClock > 0)
            {
                snapshot.Set(MetricType::GpuMemoryClock, static_cast<double>(value.memoryClock) / 100.0);
            }
        }
    }
    if (settings.IsEnabled(MetricType::GpuFanSpeed) && fanSpeed_)
    {
        FanSpeed value{sizeof(FanSpeed), kFanSpeedPercent, 0, 0};
        if (fanSpeed_(context_, adapterIndex_, 0, &value) == kAdlOk && value.fanSpeed >= 0 && value.fanSpeed <= 100)
        {
            snapshot.Set(MetricType::GpuFanSpeed, static_cast<double>(value.fanSpeed));
        }
    }
}

void AmdProvider::Reset() noexcept
{
    adapterIndex_ = -1;
    activity_ = nullptr;
    temperature_ = nullptr;
    fanSpeed_ = nullptr;
    if (context_ && destroy_)
    {
        destroy_(context_);
    }
    context_ = nullptr;
    destroy_ = nullptr;
    if (library_)
    {
        FreeLibrary(library_);
        library_ = nullptr;
    }
    attempted_ = false;
}

} // namespace statwisp
