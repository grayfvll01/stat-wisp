#include "core/MetricFormatter.h"
#include "core/SemanticVersion.h"
#include "settings/Settings.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

void Check(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void TestSettingsRoundTrip()
{
    gate::Settings input;
    input.enabled.fill(false);
    input.SetEnabled(gate::MetricType::CpuUsage, true);
    input.SetEnabled(gate::MetricType::RamUsage, true);
    input.updateIntervalMs = 30000;
    input.fahrenheit = true;
    input.compactLabels = true;
    input.roundValues = false;
    input.startWithWindows = true;
    input.firstRunCompleted = true;
    std::swap(input.order[0], input.order[4]);

    const auto serialized = gate::SettingsCodec::Serialize(input);
    bool valid = false;
    const auto output = gate::SettingsCodec::Deserialize(serialized, nullptr, &valid);
    Check(valid, "serialized settings were not accepted");
    Check(output.IsEnabled(gate::MetricType::CpuUsage), "CPU usage was not preserved");
    Check(output.IsEnabled(gate::MetricType::RamUsage), "RAM usage was not preserved");
    Check(output.EnabledCount() == 2, "enabled metric count changed");
    Check(output.updateIntervalMs == 30000 && output.fahrenheit && output.compactLabels && !output.roundValues,
          "display settings did not round trip");
    Check(output.order[0] == gate::MetricType::RamUsage, "metric order did not round trip");
}

void TestSettingsMigration()
{
    constexpr std::string_view legacy = "version=0\n"
                                        "enabled=cpu.usage,ram.usage\n"
                                        "order=ram.usage,cpu.usage,ram.usage\n"
                                        "interval=500\n"
                                        "compact=1\n"
                                        "round=0\n"
                                        "autostart=1\n"
                                        "configured=1\n";
    bool migrated = false;
    bool valid = false;
    const auto settings = gate::SettingsCodec::Deserialize(legacy, &migrated, &valid);
    Check(valid && migrated, "version 0 settings were not migrated");
    Check(settings.version == gate::kSettingsVersion, "migrated version was not normalized");
    Check(settings.order[0] == gate::MetricType::RamUsage && settings.order[1] == gate::MetricType::CpuUsage,
          "migration did not de-duplicate order");
    Check(settings.updateIntervalMs == 500 && settings.startWithWindows, "legacy aliases were not read");
}

void TestFormatting()
{
    Check(gate::MetricFormatter::FormatBytes(512.0) == L"512B", "byte formatting failed");
    Check(gate::MetricFormatter::FormatBytes(1536.0) == L"1.5K", "KiB formatting failed");
    Check(gate::MetricFormatter::FormatBytes(3.0 * 1024.0 * 1024.0, true) == L"3.0M/s", "rate formatting failed");
    Check(gate::MetricFormatter::CelsiusToFahrenheit(100.0) == 212.0, "temperature conversion failed");

    gate::Settings settings;
    auto temperature = gate::MetricFormatter::Format(gate::MetricType::CpuTemperature, 50.0, settings);
    Check(temperature.iconValue == L"50" && temperature.tooltip.find(L"50 °C") != std::wstring::npos,
          "temperature metric formatting failed");
    settings.fahrenheit = true;
    temperature = gate::MetricFormatter::Format(gate::MetricType::CpuTemperature, 50.0, settings);
    Check(temperature.iconValue == L"122", "Fahrenheit metric formatting failed");
    const auto unavailable = gate::MetricFormatter::Format(gate::MetricType::GpuTemperature, std::nullopt, settings);
    Check(unavailable.iconValue == L"--", "unsupported metric state is not explicit");
    const auto available =
        gate::MetricFormatter::Format(gate::MetricType::AvailableRam, 8.0 * 1024.0 * 1024.0 * 1024.0, settings);
    Check(available.iconValue == L"8.0G", "available RAM formatting failed");
    const auto diskRate = gate::MetricFormatter::Format(gate::MetricType::DiskRead, 2.5 * 1024.0 * 1024.0, settings);
    Check(diskRate.iconValue == L"2.5M", "disk rate formatting failed");
    const auto battery = gate::MetricFormatter::Format(gate::MetricType::BatteryPercentage, 73.0, settings);
    Check(battery.iconValue == L"73", "battery formatting failed");
    const auto fan = gate::MetricFormatter::Format(gate::MetricType::CpuFanSpeed, 1200.0, settings);
    Check(fan.iconValue == L"1.2K" && fan.tooltip.find(L"1200 RPM") != std::wstring::npos,
          "fan speed formatting failed");
}

void TestProviderActivation()
{
    gate::Settings settings;
    settings.enabled.fill(false);
    settings.SetEnabled(gate::MetricType::RamUsage, true);
    auto providers = settings.RequiredProviders();
    Check(gate::HasProvider(providers, gate::Provider::Memory), "memory provider was not activated");
    Check(!gate::HasProvider(providers, gate::Provider::Gpu), "GPU provider activated for RAM-only configuration");
    Check(!gate::HasProvider(providers, gate::Provider::Sensors),
          "thermal provider activated for RAM-only configuration");

    settings.SetEnabled(gate::MetricType::GpuTemperature, true);
    providers = settings.RequiredProviders();
    Check(gate::HasProvider(providers, gate::Provider::Gpu), "GPU provider was not activated");

    settings.SetEnabled(gate::MetricType::CpuFanSpeed, true);
    providers = settings.RequiredProviders();
    Check(gate::HasProvider(providers, gate::Provider::Sensors), "sensor provider was not activated for CPU fan");

    settings.enabled.fill(false);
    settings.SetEnabled(gate::MetricType::DiskRead, true);
    settings.SetEnabled(gate::MetricType::BatteryPercentage, true);
    providers = settings.RequiredProviders();
    Check(gate::HasProvider(providers, gate::Provider::Disk), "disk provider was not activated");
    Check(gate::HasProvider(providers, gate::Provider::Battery), "battery provider was not activated");
    Check(!gate::HasProvider(providers, gate::Provider::Gpu), "GPU provider activated for disk/battery configuration");

    settings.updateIntervalMs = 60000;
    settings.Normalize();
    Check(settings.updateIntervalMs == 60000, "one-minute interval was not accepted");
}

void TestVersionParsing()
{
    const auto version = gate::SemanticVersion::Parse("v0.1.23");
    Check(version.has_value(), "valid tag version was rejected");
    Check(*version == gate::SemanticVersion{0, 1, 23}, "version fields were parsed incorrectly");
    Check(version->ToString() == "0.1.23", "version string formatting failed");
    Check(!gate::SemanticVersion::Parse("1.2").has_value(), "incomplete version was accepted");
    Check(!gate::SemanticVersion::Parse("1.x.3").has_value(), "invalid version was accepted");
}

} // namespace

int main()
{
    try
    {
        TestSettingsRoundTrip();
        TestSettingsMigration();
        TestFormatting();
        TestProviderActivation();
        TestVersionParsing();
        std::cout << "gate-monitor core tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "gate-monitor core test failure: " << error.what() << '\n';
        return 1;
    }
}
