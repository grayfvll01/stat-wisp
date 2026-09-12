#include "core/MetricFormatter.h"
#include "core/GpuEngineUsage.h"
#include "core/NetworkRateTracker.h"
#include "core/SemanticVersion.h"
#include "settings/Settings.h"

#include <iostream>
#include <fstream>
#include <limits>
#include <Windows.h>
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
    statwisp::Settings input;
    input.enabled.fill(false);
    input.SetEnabled(statwisp::MetricType::CpuUsage, true);
    input.SetEnabled(statwisp::MetricType::RamUsage, true);
    input.updateIntervalMs = 30000;
    input.fahrenheit = true;
    input.compactLabels = true;
    input.roundValues = false;
    input.startWithWindows = true;
    input.firstRunCompleted = true;
    std::swap(input.order[0], input.order[4]);

    const auto serialized = statwisp::SettingsCodec::Serialize(input);
    bool valid = false;
    const auto output = statwisp::SettingsCodec::Deserialize(serialized, nullptr, &valid);
    Check(valid, "serialized settings were not accepted");
    Check(output.IsEnabled(statwisp::MetricType::CpuUsage), "CPU usage was not preserved");
    Check(output.IsEnabled(statwisp::MetricType::RamUsage), "RAM usage was not preserved");
    Check(output.EnabledCount() == 2, "enabled metric count changed");
    Check(output.updateIntervalMs == 30000 && output.fahrenheit && output.compactLabels && !output.roundValues,
          "display settings did not round trip");
    Check(output.order[0] == statwisp::MetricType::RamUsage, "metric order did not round trip");
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
    const auto settings = statwisp::SettingsCodec::Deserialize(legacy, &migrated, &valid);
    Check(valid && migrated, "version 0 settings were not migrated");
    Check(settings.version == statwisp::kSettingsVersion, "migrated version was not normalized");
    Check(settings.order[0] == statwisp::MetricType::RamUsage && settings.order[1] == statwisp::MetricType::CpuUsage,
          "migration did not de-duplicate order");
    Check(settings.updateIntervalMs == 500 && settings.startWithWindows, "legacy aliases were not read");
}

void TestFormatting()
{
    Check(statwisp::MetricFormatter::FormatBytes(512.0) == L"512B", "byte formatting failed");
    Check(statwisp::MetricFormatter::FormatBytes(1536.0) == L"1.5K", "KiB formatting failed");
    Check(statwisp::MetricFormatter::FormatBytes(3.0 * 1024.0 * 1024.0, true) == L"3.0M/s", "rate formatting failed");
    Check(statwisp::MetricFormatter::CelsiusToFahrenheit(100.0) == 212.0, "temperature conversion failed");

    statwisp::Settings settings;
    auto temperature = statwisp::MetricFormatter::Format(statwisp::MetricType::CpuTemperature, 50.0, settings);
    Check(temperature.iconValue == L"50" && temperature.tooltip.find(L"50 °C") != std::wstring::npos,
          "temperature metric formatting failed");
    settings.fahrenheit = true;
    temperature = statwisp::MetricFormatter::Format(statwisp::MetricType::CpuTemperature, 50.0, settings);
    Check(temperature.iconValue == L"122", "Fahrenheit metric formatting failed");
    const auto unavailable = statwisp::MetricFormatter::Format(statwisp::MetricType::GpuTemperature, std::nullopt, settings);
    Check(unavailable.iconValue == L"--", "unsupported metric state is not explicit");
    const auto available =
        statwisp::MetricFormatter::Format(statwisp::MetricType::AvailableRam, 8.0 * 1024.0 * 1024.0 * 1024.0, settings);
    Check(available.iconValue == L"8.0G", "available RAM formatting failed");
    const auto diskRate = statwisp::MetricFormatter::Format(statwisp::MetricType::DiskRead, 2.5 * 1024.0 * 1024.0, settings);
    Check(diskRate.iconValue == L"2.5M", "disk rate formatting failed");
    const auto battery = statwisp::MetricFormatter::Format(statwisp::MetricType::BatteryPercentage, 73.0, settings);
    Check(battery.iconValue == L"73", "battery formatting failed");
    const auto fan = statwisp::MetricFormatter::Format(statwisp::MetricType::CpuFanSpeed, 1200.0, settings);
    Check(fan.iconValue == L"1.2K" && fan.tooltip.find(L"1200 RPM") != std::wstring::npos,
          "fan speed formatting failed");
}

void TestProviderActivation()
{
    statwisp::Settings settings;
    settings.enabled.fill(false);
    settings.SetEnabled(statwisp::MetricType::RamUsage, true);
    auto providers = settings.RequiredProviders();
    Check(statwisp::HasProvider(providers, statwisp::Provider::Memory), "memory provider was not activated");
    Check(!statwisp::HasProvider(providers, statwisp::Provider::Gpu), "GPU provider activated for RAM-only configuration");
    Check(!statwisp::HasProvider(providers, statwisp::Provider::Sensors),
          "thermal provider activated for RAM-only configuration");

    settings.SetEnabled(statwisp::MetricType::GpuTemperature, true);
    providers = settings.RequiredProviders();
    Check(statwisp::HasProvider(providers, statwisp::Provider::Gpu), "GPU provider was not activated");

    settings.SetEnabled(statwisp::MetricType::CpuFanSpeed, true);
    providers = settings.RequiredProviders();
    Check(statwisp::HasProvider(providers, statwisp::Provider::Sensors), "sensor provider was not activated for CPU fan");

    settings.enabled.fill(false);
    settings.SetEnabled(statwisp::MetricType::DiskRead, true);
    settings.SetEnabled(statwisp::MetricType::BatteryPercentage, true);
    providers = settings.RequiredProviders();
    Check(statwisp::HasProvider(providers, statwisp::Provider::Disk), "disk provider was not activated");
    Check(statwisp::HasProvider(providers, statwisp::Provider::Battery), "battery provider was not activated");
    Check(!statwisp::HasProvider(providers, statwisp::Provider::Gpu), "GPU provider activated for disk/battery configuration");

    settings.updateIntervalMs = 60000;
    settings.Normalize();
    Check(settings.updateIntervalMs == 60000, "one-minute interval was not accepted");
}

void TestVersionParsing()
{
    const auto version = statwisp::SemanticVersion::Parse("v0.1.23");
    Check(version.has_value(), "valid tag version was rejected");
    Check(*version == statwisp::SemanticVersion{0, 1, 23}, "version fields were parsed incorrectly");
    Check(version->ToString() == "0.1.23", "version string formatting failed");
    Check(!statwisp::SemanticVersion::Parse("1.2").has_value(), "incomplete version was accepted");
    Check(!statwisp::SemanticVersion::Parse("1.x.3").has_value(), "invalid version was accepted");
}

void TestNetworkRates()
{
    statwisp::NetworkRateTracker tracker;
    const auto start = std::chrono::steady_clock::time_point{};
    Check(!tracker.Sample({{1, 1000, 2000}}, start), "first network sample had no baseline");
    auto rates = tracker.Sample({{1, 1200, 2400}}, start + std::chrono::seconds(2));
    Check(rates && rates->download == 100.0 && rates->upload == 200.0, "network rate calculation failed");

    rates = tracker.Sample({{1, 1300, 2600}, {2, 1000000000, 2000000000}}, start + std::chrono::seconds(3));
    Check(rates && rates->download == 100.0 && rates->upload == 200.0,
          "connecting an interface counted its lifetime traffic");
    rates = tracker.Sample({{2, 1000000300, 2000000500}, {1, 1500, 2800}}, start + std::chrono::seconds(4));
    Check(rates && rates->download == 500.0 && rates->upload == 700.0,
          "interface rates were not aggregated independently of enumeration order");
    rates = tracker.Sample({{1, 1700, 3100}}, start + std::chrono::seconds(5));
    Check(rates && rates->download == 200.0 && rates->upload == 300.0,
          "disconnecting an interface discarded traffic on a remaining interface");

    rates = tracker.Sample({{1, 20, 3300}}, start + std::chrono::seconds(6));
    Check(rates && rates->download == 0.0 && rates->upload == 200.0,
          "resetting a download counter corrupted the independent upload rate");
    rates = tracker.Sample({{1, 120, 3400}, {2, 1000000900, 2000000900}}, start + std::chrono::seconds(7));
    Check(rates && rates->download == 100.0 && rates->upload == 100.0,
          "reconnecting an interface used a stale baseline");
    rates = tracker.Sample({}, start + std::chrono::seconds(8));
    Check(rates && rates->download == 0.0 && rates->upload == 0.0, "offline network did not report zero traffic");

    tracker.Reset();
    Check(!tracker.Sample({{1, 1000, 2000}}, start + std::chrono::seconds(9)),
          "reset network tracker retained a stale baseline");
}

void TestGpuEngineUsage()
{
    statwisp::GpuEngineUsage engines;
    Check(!engines.Busiest(), "missing GPU counters were reported as idle");
    engines.Add(L"unrecognized", 100.0);
    engines.Add(L"pid_1_luid_0x0_0x1234_phys_0_eng_0_engtype_3D", std::numeric_limits<double>::quiet_NaN());
    engines.Add(L"pid_1_luid_0x0_0x1234_phys_0_eng_0_engtype_3D", -1.0);
    Check(!engines.Busiest(), "invalid GPU counters were accepted");
    engines.Add(L"pid_1_luid_0x0_0x1234_phys_0_eng_0_engtype_3D", 40.0);
    engines.Add(L"pid_2_luid_0x0_0x1234_phys_0_eng_0_engtype_3D", 35.0);
    Check(engines.Busiest() == 75.0, "GPU usage did not aggregate processes sharing an engine");
    engines.Add(L"pid_1_luid_0x0_0x1234_phys_0_eng_1_engtype_Copy", 60.0);
    engines.Add(L"pid_1_luid_0x0_0x5678_phys_0_eng_0_engtype_3D", 50.0);
    engines.Add(L"pid_1_luid_0x0_0x1234_phys_1_eng_0_engtype_3D", 55.0);
    Check(engines.Busiest() == 75.0, "independent GPU engines or adapters were combined");
    engines.Add(L"pid_3_luid_0x0_0x1234_phys_0_eng_0_engtype_3D", 30.0);
    Check(engines.Busiest() == 100.0, "GPU usage was not capped after aggregation");

    statwisp::GpuEngineUsage idle;
    idle.Add(L"pid_1_luid_0x0_0x1234_phys_0_eng_0_engtype_3D", 0.0);
    Check(idle.Busiest() == 0.0, "valid idle GPU counter was reported as unavailable");
}

} // namespace

void TestBoundedSettingsRead()
{
    const auto path = std::filesystem::temp_directory_path() /
                      ("stat-wisp-test-" + std::to_string(GetCurrentProcessId()) + ".ini");
    statwisp::SettingsStore store(path);
    statwisp::Settings settings;
    settings.firstRunCompleted = true;
    Check(store.Save(settings), "settings save failed");
    bool recovered = true;
    Check(store.Load(&recovered).firstRunCompleted && !recovered, "valid settings load failed");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << std::string(65537, 'x');
    }
    Check(!store.Load(&recovered).firstRunCompleted && recovered, "oversized settings were accepted");
    std::filesystem::remove(path);
}

int main()
{
    try
    {
        TestSettingsRoundTrip();
        TestSettingsMigration();
        TestFormatting();
        TestProviderActivation();
        TestVersionParsing();
        TestNetworkRates();
        TestGpuEngineUsage();
        TestBoundedSettingsRead();
        std::cout << "stat-wisp core tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "stat-wisp core test failure: " << error.what() << '\n';
        return 1;
    }
}
