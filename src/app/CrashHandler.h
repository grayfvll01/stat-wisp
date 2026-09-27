#pragma once

#include <filesystem>

namespace statwisp
{

// Installs process-wide handlers for unhandled SEH exceptions, std::terminate,
// pure virtual calls, and invalid CRT parameters. Each writes a small minidump
// (at most three are kept) and a log entry into `directory`, then ends the
// process. Safe to call once, early in wWinMain.
void InstallCrashHandler(const std::filesystem::path &directory) noexcept;

} // namespace statwisp
