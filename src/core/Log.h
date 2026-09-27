#pragma once

#include <filesystem>
#include <string_view>

namespace statwisp
{

enum class LogLevel
{
    Info,
    Warning,
    Error,
};

// A small, bounded, thread-safe diagnostic log (stat-wisp.log beside the
// settings file). Logging never throws and never blocks on anything slower
// than a local file append; failures are silently ignored.
void InitializeLog(const std::filesystem::path &directory) noexcept;
void WriteLog(LogLevel level, std::wstring_view message) noexcept;
[[nodiscard]] std::filesystem::path LogFilePath() noexcept;

inline void LogInfo(std::wstring_view message) noexcept
{
    WriteLog(LogLevel::Info, message);
}

inline void LogWarning(std::wstring_view message) noexcept
{
    WriteLog(LogLevel::Warning, message);
}

inline void LogError(std::wstring_view message) noexcept
{
    WriteLog(LogLevel::Error, message);
}

} // namespace statwisp
