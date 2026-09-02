#pragma once

#include "monitoring/Metric.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace gate
{

inline constexpr std::uint32_t kSettingsVersion = 2;
inline constexpr std::array<std::uint32_t, 7> kAllowedUpdateIntervals{500, 1000, 2000, 5000, 10000, 30000, 60000};

struct Settings
{
    std::uint32_t version{kSettingsVersion};
    std::array<bool, kMetricCount> enabled{};
    std::array<MetricType, kMetricCount> order{};
    std::uint32_t updateIntervalMs{1000};
    bool fahrenheit{false};
    bool compactLabels{false};
    bool roundValues{true};
    bool startWithWindows{false};
    bool diagnostics{false};
    bool firstRunCompleted{false};

    Settings();
    [[nodiscard]] std::size_t EnabledCount() const noexcept;
    [[nodiscard]] Provider RequiredProviders() const noexcept;
    [[nodiscard]] bool IsEnabled(MetricType type) const noexcept
    {
        return enabled[MetricIndex(type)];
    }
    void SetEnabled(MetricType type, bool value) noexcept
    {
        enabled[MetricIndex(type)] = value;
    }
    void Normalize() noexcept;
};

class SettingsStore final
{
  public:
    SettingsStore();
    explicit SettingsStore(std::filesystem::path path);

    [[nodiscard]] bool Exists() const noexcept;
    [[nodiscard]] Settings Load(bool *recoveredFromError = nullptr) const;
    [[nodiscard]] bool Save(const Settings &settings) const;
    [[nodiscard]] const std::filesystem::path &Path() const noexcept
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

class SettingsCodec final
{
  public:
    [[nodiscard]] static std::string Serialize(const Settings &settings);
    [[nodiscard]] static Settings Deserialize(std::string_view text, bool *migrated = nullptr, bool *valid = nullptr);
};

} // namespace gate
