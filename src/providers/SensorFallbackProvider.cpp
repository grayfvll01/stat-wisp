#include "providers/SensorFallbackProvider.h"

#include <Wbemidl.h>
#include <Windows.h>
#include <comdef.h>



#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace statwisp
{
namespace
{

struct SensorReadings
{
    std::optional<double> cpuTemperature;
    std::optional<double> gpuTemperature;
    std::optional<double> cpuFan;
    std::optional<double> systemFan;
    int cpuTemperatureScore{-1};
    int gpuTemperatureScore{-1};
};

std::wstring Lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return value;
}

bool Contains(std::wstring_view value, std::wstring_view needle) noexcept
{
    return value.find(needle) != std::wstring_view::npos;
}

std::wstring StringProperty(IWbemClassObject *object, const wchar_t *name)
{
    VARIANT value{};
    VariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr)) && value.vt == VT_BSTR && value.bstrVal)
    {
        result.assign(value.bstrVal, SysStringLen(value.bstrVal));
    }
    VariantClear(&value);
    return result;
}

std::optional<double> NumberProperty(IWbemClassObject *object, const wchar_t *name)
{
    VARIANT value{};
    VariantInit(&value);
    if (FAILED(object->Get(name, 0, &value, nullptr, nullptr)))
    {
        VariantClear(&value);
        return std::nullopt;
    }
    VARIANT converted{};
    VariantInit(&converted);
    const auto result = VariantChangeType(&converted, &value, 0, VT_R8);
    VariantClear(&value);
    if (FAILED(result))
    {
        VariantClear(&converted);
        return std::nullopt;
    }
    const auto number = converted.dblVal;
    VariantClear(&converted);
    return std::isfinite(number) ? std::optional<double>(number) : std::nullopt;
}

int CpuTemperatureScore(std::wstring_view name) noexcept
{
    if (Contains(name, L"package"))
    {
        return 100;
    }
    if (Contains(name, L"tctl") || Contains(name, L"tdie"))
    {
        return 95;
    }
    if (Contains(name, L"die"))
    {
        return 90;
    }
    if (Contains(name, L"average"))
    {
        return 85;
    }
    if (Contains(name, L"max"))
    {
        return 80;
    }
    return 50;
}

int GpuTemperatureScore(std::wstring_view name) noexcept
{
    if (Contains(name, L"core"))
    {
        return 100;
    }
    if (Contains(name, L"hot spot") || Contains(name, L"hotspot"))
    {
        return 90;
    }
    if (Contains(name, L"memory"))
    {
        return 60;
    }
    return 70;
}

void AddExternalSensor(SensorReadings &readings, std::wstring identifier, std::wstring name, std::wstring sensorType,
                       double value)
{
    identifier = Lower(std::move(identifier));
    name = Lower(std::move(name));
    sensorType = Lower(std::move(sensorType));

    const bool gpu = Contains(identifier, L"/gpu-") || Contains(identifier, L"\\gpu-") ||
                     Contains(name, L"gpu");
    const bool cpu = Contains(identifier, L"/intelcpu/") || Contains(identifier, L"/amdcpu/") ||
                     Contains(identifier, L"/cpu/") || Contains(name, L"cpu");

    if (sensorType == L"temperature" && value >= 0.0 && value <= 130.0)
    {
        if (gpu)
        {
            const auto score = GpuTemperatureScore(name);
            if (score > readings.gpuTemperatureScore)
            {
                readings.gpuTemperature = value;
                readings.gpuTemperatureScore = score;
            }
        }
        else if (cpu)
        {
            const auto score = CpuTemperatureScore(name);
            if (score > readings.cpuTemperatureScore)
            {
                readings.cpuTemperature = value;
                readings.cpuTemperatureScore = score;
            }
        }
    }
    else if (sensorType == L"fan" && value >= 0.0 && value <= 100000.0 && !gpu)
    {
        if (Contains(name, L"cpu"))
        {
            readings.cpuFan = std::max(readings.cpuFan.value_or(0.0), value);
        }
        else
        {
            readings.systemFan = std::max(readings.systemFan.value_or(0.0), value);
        }
    }
}

void ReleaseService(IWbemServices *&service) noexcept
{
    if (service)
    {
        service->Release();
        service = nullptr;
    }
}

} // namespace

struct SensorFallbackProvider::Impl
{
    IWbemLocator *locator{};
    IWbemServices *libreHardwareMonitor{};
    IWbemServices *openHardwareMonitor{};



    std::chrono::steady_clock::time_point nextExternalConnect{};

    ~Impl()
    {
        ReleaseService(libreHardwareMonitor);
        ReleaseService(openHardwareMonitor);

        if (locator)
        {
            locator->Release();
        }

    }

    bool Connect(const wchar_t *name, IWbemServices **service)
    {
        if (!locator || *service)
        {
            return *service != nullptr;
        }
        auto result = locator->ConnectServer(_bstr_t(name), nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                             nullptr, nullptr, service);
        if (FAILED(result) || !*service)
        {
            *service = nullptr;
            return false;
        }
        result = CoSetProxyBlanket(*service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                   RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(result))
        {
            ReleaseService(*service);
            return false;
        }
        return true;
    }

    bool Initialize()
    {
        return SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                          reinterpret_cast<void **>(&locator)));
    }
    bool QueryExternal(IWbemServices *&service, SensorReadings &readings)
    {
        if (!service)
        {
            return false;
        }
        IEnumWbemClassObject *enumerator{};
        const auto result = service->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(L"SELECT Identifier, Name, SensorType, Value FROM Sensor"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (FAILED(result) || !enumerator)
        {
            ReleaseService(service);
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        for (unsigned int rows = 0; rows < 4096 && std::chrono::steady_clock::now() < deadline; ++rows)
        {
            IWbemClassObject *object{};
            ULONG returned = 0;
            const auto next = enumerator->Next(500, 1, &object, &returned);
            if (next != WBEM_S_NO_ERROR || returned == 0)
            {
                if (object) object->Release();
                break;
            }
            const auto value = NumberProperty(object, L"Value");
            if (value)
            {
                AddExternalSensor(readings, StringProperty(object, L"Identifier"), StringProperty(object, L"Name"),
                                  StringProperty(object, L"SensorType"), *value);
            }
            object->Release();
        }
        enumerator->Release();
        return true;
    }

    SensorReadings Query()
    {
        SensorReadings readings;
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextExternalConnect)
        {
            Connect(L"ROOT\\LibreHardwareMonitor", &libreHardwareMonitor);
            Connect(L"ROOT\\OpenHardwareMonitor", &openHardwareMonitor);
            nextExternalConnect = now + std::chrono::seconds(15);
        }
        QueryExternal(libreHardwareMonitor, readings);
        QueryExternal(openHardwareMonitor, readings);


        return readings;
    }
};

SensorFallbackProvider::SensorFallbackProvider() = default;
SensorFallbackProvider::~SensorFallbackProvider() = default;

void SensorFallbackProvider::Collect(MetricSnapshot &snapshot, const Settings &settings)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextRead_)
    {
        nextRead_ = now + std::chrono::seconds(2);
        if (!impl_ && now >= nextInitialize_)
        {
            auto candidate = std::make_unique<Impl>();
            if (candidate->Initialize())
            {
                impl_ = std::move(candidate);
            }
            else
            {
                nextInitialize_ = now + std::chrono::seconds(30);
            }
        }
        cached_.values.fill(std::nullopt);
        if (impl_)
        {
            const auto readings = impl_->Query();
            if (readings.cpuTemperature)
            {
                cached_.Set(MetricType::CpuTemperature, *readings.cpuTemperature);
            }
            if (readings.gpuTemperature)
            {
                cached_.Set(MetricType::GpuTemperature, *readings.gpuTemperature);
            }
            if (readings.cpuFan)
            {
                cached_.Set(MetricType::CpuFanSpeed, *readings.cpuFan);
            }
            if (readings.systemFan)
            {
                cached_.Set(MetricType::SystemFanSpeed, *readings.systemFan);
            }
        }
    }

    constexpr std::array sensorMetrics{MetricType::CpuTemperature, MetricType::GpuTemperature,
                                       MetricType::CpuFanSpeed, MetricType::SystemFanSpeed};
    for (const auto type : sensorMetrics)
    {
        if (settings.IsEnabled(type) && !snapshot.Get(type))
        {
            if (const auto value = cached_.Get(type))
            {
                snapshot.Set(type, *value);
            }
        }
    }
}

void SensorFallbackProvider::Reset() noexcept
{
    impl_.reset();
    cached_ = {};
    nextRead_ = {};
    nextInitialize_ = {};
}

} // namespace statwisp
