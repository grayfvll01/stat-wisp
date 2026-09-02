#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace gate
{

enum class MetricType : std::uint8_t
{
    CpuTemperature,
    GpuTemperature,
    CpuUsage,
    GpuUsage,
    RamUsage,
    AvailableRam,
    CommitUsage,
    CpuClock,
    GpuClock,
    GpuMemoryClock,
    VramUsage,
    NetworkDownload,
    NetworkUpload,
    DiskRead,
    DiskWrite,
    DiskUtilization,
    BatteryPercentage,
    CpuFanSpeed,
    GpuFanSpeed,
    SystemFanSpeed,
    Count
};

constexpr std::size_t kMetricCount = static_cast<std::size_t>(MetricType::Count);

enum class Provider : std::uint32_t
{
    None = 0,
    CpuNative = 1U << 0U,
    Sensors = 1U << 1U,
    Memory = 1U << 2U,
    Gpu = 1U << 3U,
    Network = 1U << 4U,
    Disk = 1U << 5U,
    Battery = 1U << 6U,
};

constexpr Provider operator|(Provider left, Provider right) noexcept
{
    return static_cast<Provider>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr Provider &operator|=(Provider &left, Provider right) noexcept
{
    left = left | right;
    return left;
}

constexpr bool HasProvider(Provider set, Provider value) noexcept
{
    return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(value)) != 0;
}

struct MetricDefinition
{
    MetricType type;
    std::string_view key;
    std::wstring_view name;
    std::wstring_view shortLabel;
    Provider provider;
};

inline constexpr std::array<MetricDefinition, kMetricCount> kMetricDefinitions{{
    {MetricType::CpuTemperature, "cpu.temperature", L"CPU Temperature", L"CT", Provider::Sensors},
    {MetricType::GpuTemperature, "gpu.temperature", L"GPU Temperature", L"GT", Provider::Gpu},
    {MetricType::CpuUsage, "cpu.usage", L"CPU Usage", L"CU", Provider::CpuNative},
    {MetricType::GpuUsage, "gpu.usage", L"GPU Usage", L"GU", Provider::Gpu},
    {MetricType::RamUsage, "ram.usage", L"RAM Usage", L"RU", Provider::Memory},
    {MetricType::AvailableRam, "ram.available", L"Available RAM", L"RA", Provider::Memory},
    {MetricType::CommitUsage, "memory.commit", L"Commit Usage", L"CM", Provider::Memory},
    {MetricType::CpuClock, "cpu.clock", L"CPU Clock", L"CC", Provider::CpuNative},
    {MetricType::GpuClock, "gpu.clock", L"GPU Clock", L"GC", Provider::Gpu},
    {MetricType::GpuMemoryClock, "gpu.memory_clock", L"GPU Memory Clock", L"GM", Provider::Gpu},
    {MetricType::VramUsage, "vram.usage", L"VRAM Usage", L"VR", Provider::Gpu},
    {MetricType::NetworkDownload, "network.download", L"Network Download", L"ND", Provider::Network},
    {MetricType::NetworkUpload, "network.upload", L"Network Upload", L"NU", Provider::Network},
    {MetricType::DiskRead, "disk.read", L"Disk Read", L"DR", Provider::Disk},
    {MetricType::DiskWrite, "disk.write", L"Disk Write", L"DW", Provider::Disk},
    {MetricType::DiskUtilization, "disk.usage", L"Disk Activity", L"DU", Provider::Disk},
    {MetricType::BatteryPercentage, "battery.percentage", L"Battery", L"BT", Provider::Battery},
    {MetricType::CpuFanSpeed, "cpu.fan", L"CPU Fan (RPM)", L"CF", Provider::Sensors},
    {MetricType::GpuFanSpeed, "gpu.fan", L"GPU Fan (%)", L"GF", Provider::Gpu},
    {MetricType::SystemFanSpeed, "system.fan", L"System Fan (RPM)", L"SF", Provider::Sensors},
}};

constexpr std::size_t MetricIndex(MetricType type) noexcept
{
    return static_cast<std::size_t>(type);
}

constexpr const MetricDefinition &Definition(MetricType type) noexcept
{
    return kMetricDefinitions[MetricIndex(type)];
}

inline std::optional<MetricType> MetricFromKey(std::string_view key) noexcept
{
    for (const auto &definition : kMetricDefinitions)
    {
        if (definition.key == key)
        {
            return definition.type;
        }
    }
    return std::nullopt;
}

} // namespace gate
