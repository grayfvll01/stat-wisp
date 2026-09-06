#include "settings/Settings.h"

#include <ShlObj.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace statwisp
{
namespace
{

bool ParseBool(std::string_view value, bool fallback) noexcept
{
    if (value == "1" || value == "true")
    {
        return true;
    }
    if (value == "0" || value == "false")
    {
        return false;
    }
    return fallback;
}

std::uint32_t ParseUnsigned(std::string_view value, std::uint32_t fallback) noexcept
{
    std::uint32_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() ? result : fallback;
}

template <typename Callback> void ForEachToken(std::string_view text, Callback &&callback)
{
    std::size_t start = 0;
    while (start <= text.size())
    {
        const auto end = text.find(',', start);
        const auto token = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        if (!token.empty())
        {
            callback(token);
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        start = end + 1;
    }
}

std::filesystem::path DefaultSettingsPath()
{
    std::array<wchar_t, 32768> overridePath{};
    const auto overrideLength = GetEnvironmentVariableW(L"STAT_WISP_SETTINGS_PATH", overridePath.data(),
                                                        static_cast<DWORD>(overridePath.size()));
    if (overrideLength != 0 && overrideLength < overridePath.size())
    {
        return std::filesystem::path(std::wstring(overridePath.data(), overrideLength));
    }
    PWSTR localAppData{};
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData)))
    {
        std::filesystem::path path(localAppData);
        CoTaskMemFree(localAppData);
        return path / L"stat-wisp" / L"settings.ini";
    }
    return std::filesystem::temp_directory_path() / L"stat-wisp" / L"settings.ini";
}

} // namespace

Settings::Settings()
{
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        order[index] = static_cast<MetricType>(index);
    }
    SetEnabled(MetricType::CpuTemperature, true);
    SetEnabled(MetricType::GpuTemperature, true);
}

std::size_t Settings::EnabledCount() const noexcept
{
    return static_cast<std::size_t>(std::count(enabled.begin(), enabled.end(), true));
}

Provider Settings::RequiredProviders() const noexcept
{
    Provider providers = Provider::None;
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        if (enabled[index])
        {
            providers |= kMetricDefinitions[index].provider;
        }
    }
    return providers;
}

void Settings::Normalize() noexcept
{
    version = kSettingsVersion;
    if (std::find(kAllowedUpdateIntervals.begin(), kAllowedUpdateIntervals.end(), updateIntervalMs) ==
        kAllowedUpdateIntervals.end())
    {
        updateIntervalMs = 1000;
    }

    std::array<bool, kMetricCount> seen{};
    std::array<MetricType, kMetricCount> normalized{};
    std::size_t output = 0;
    for (const auto type : order)
    {
        const auto index = MetricIndex(type);
        if (index < kMetricCount && !seen[index])
        {
            normalized[output++] = type;
            seen[index] = true;
        }
    }
    for (std::size_t index = 0; index < kMetricCount; ++index)
    {
        if (!seen[index])
        {
            normalized[output++] = static_cast<MetricType>(index);
        }
    }
    order = normalized;
}

SettingsStore::SettingsStore() : path_(DefaultSettingsPath())
{
}

SettingsStore::SettingsStore(std::filesystem::path path) : path_(std::move(path))
{
}

bool SettingsStore::Exists() const noexcept
{
    std::error_code error;
    return std::filesystem::is_regular_file(path_, error);
}

Settings SettingsStore::Load(bool *recoveredFromError) const
{
    if (recoveredFromError)
    {
        *recoveredFromError = false;
    }
    std::ifstream stream(path_, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    // Settings are normally under 1 KiB. Bound reads even if the file is corrupt
    // or grows while it is being read.
    constexpr std::size_t maxSettingsBytes = 64 * 1024;
    std::string contents(maxSettingsBytes + 1, '\0');
    stream.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    const auto bytesRead = static_cast<std::size_t>(stream.gcount());
    if (bytesRead > maxSettingsBytes || stream.bad())
    {
        if (recoveredFromError) *recoveredFromError = true;
        return {};
    }
    contents.resize(bytesRead);
    bool valid = false;
    auto settings = SettingsCodec::Deserialize(contents, nullptr, &valid);
    if (!valid && recoveredFromError)
    {
        *recoveredFromError = true;
    }
    return settings;
}

bool SettingsStore::Save(const Settings &source) const
{
    auto settings = source;
    settings.Normalize();
    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);
    if (error)
    {
        return false;
    }

    auto temporary = path_;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        const auto serialized = SettingsCodec::Serialize(settings);
        stream.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        if (!stream)
        {
            return false;
        }
    }

    if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

std::string SettingsCodec::Serialize(const Settings &source)
{
    auto settings = source;
    settings.Normalize();
    std::ostringstream output;
    output << "version=" << kSettingsVersion << '\n';
    output << "enabled=";
    bool first = true;
    for (const auto type : settings.order)
    {
        if (settings.IsEnabled(type))
        {
            if (!first)
            {
                output << ',';
            }
            output << Definition(type).key;
            first = false;
        }
    }
    output << "\norder=";
    for (std::size_t index = 0; index < settings.order.size(); ++index)
    {
        if (index != 0)
        {
            output << ',';
        }
        output << Definition(settings.order[index]).key;
    }
    output << "\ninterval_ms=" << settings.updateIntervalMs;
    output << "\nfahrenheit=" << (settings.fahrenheit ? 1 : 0);
    output << "\ncompact_labels=" << (settings.compactLabels ? 1 : 0);
    output << "\nround_values=" << (settings.roundValues ? 1 : 0);
    output << "\nstart_with_windows=" << (settings.startWithWindows ? 1 : 0);
    output << "\ndiagnostics=" << (settings.diagnostics ? 1 : 0);
    output << "\nfirst_run_completed=" << (settings.firstRunCompleted ? 1 : 0) << '\n';
    return output.str();
}

Settings SettingsCodec::Deserialize(std::string_view text, bool *migrated, bool *valid)
{
    Settings settings;
    if (migrated)
    {
        *migrated = false;
    }
    if (valid)
    {
        *valid = false;
    }

    bool sawVersion = false;
    bool sawEnabled = false;
    std::uint32_t sourceVersion = 0;
    std::array<MetricType, kMetricCount> parsedOrder{};
    std::array<bool, kMetricCount> orderSeen{};
    std::size_t orderCount = 0;

    std::size_t lineStart = 0;
    while (lineStart < text.size())
    {
        const auto lineEnd = text.find('\n', lineStart);
        auto line =
            text.substr(lineStart, lineEnd == std::string_view::npos ? text.size() - lineStart : lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        const auto separator = line.find('=');
        if (separator != std::string_view::npos)
        {
            const auto key = line.substr(0, separator);
            const auto value = line.substr(separator + 1);
            if (key == "version")
            {
                sourceVersion = ParseUnsigned(value, 0);
                sawVersion = true;
            }
            else if (key == "enabled")
            {
                settings.enabled.fill(false);
                ForEachToken(value, [&](std::string_view token) {
                    if (const auto metric = MetricFromKey(token))
                    {
                        settings.SetEnabled(*metric, true);
                    }
                });
                sawEnabled = true;
            }
            else if (key == "order")
            {
                ForEachToken(value, [&](std::string_view token) {
                    if (const auto metric = MetricFromKey(token))
                    {
                        const auto index = MetricIndex(*metric);
                        if (!orderSeen[index] && orderCount < kMetricCount)
                        {
                            parsedOrder[orderCount++] = *metric;
                            orderSeen[index] = true;
                        }
                    }
                });
            }
            else if (key == "interval_ms" || key == "interval")
            {
                settings.updateIntervalMs = ParseUnsigned(value, settings.updateIntervalMs);
            }
            else if (key == "fahrenheit")
            {
                settings.fahrenheit = ParseBool(value, settings.fahrenheit);
            }
            else if (key == "compact_labels" || key == "compact")
            {
                settings.compactLabels = ParseBool(value, settings.compactLabels);
            }
            else if (key == "round_values" || key == "round")
            {
                settings.roundValues = ParseBool(value, settings.roundValues);
            }
            else if (key == "start_with_windows" || key == "autostart")
            {
                settings.startWithWindows = ParseBool(value, settings.startWithWindows);
            }
            else if (key == "diagnostics")
            {
                settings.diagnostics = ParseBool(value, settings.diagnostics);
            }
            else if (key == "first_run_completed" || key == "configured")
            {
                settings.firstRunCompleted = ParseBool(value, settings.firstRunCompleted);
            }
        }
        if (lineEnd == std::string_view::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }

    if (orderCount != 0)
    {
        for (std::size_t index = 0; index < kMetricCount; ++index)
        {
            if (!orderSeen[index])
            {
                parsedOrder[orderCount++] = static_cast<MetricType>(index);
            }
        }
        settings.order = parsedOrder;
    }

    const bool recognized = sawVersion && sourceVersion <= kSettingsVersion && sawEnabled;
    if (!recognized)
    {
        settings.firstRunCompleted = false;
    }
    if (migrated)
    {
        *migrated = recognized && sourceVersion < kSettingsVersion;
    }
    settings.Normalize();
    if (valid)
    {
        *valid = recognized;
    }
    return settings;
}

} // namespace statwisp
