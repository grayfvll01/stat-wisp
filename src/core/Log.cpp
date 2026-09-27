#include "core/Log.h"

namespace statwisp
{

// Interface placeholder; replaced by the bounded file logger.
void InitializeLog(const std::filesystem::path &) noexcept
{
}

void WriteLog(LogLevel, std::wstring_view) noexcept
{
}

std::filesystem::path LogFilePath() noexcept
{
    return {};
}

} // namespace statwisp
