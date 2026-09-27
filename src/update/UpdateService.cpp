#include "update/UpdateService.h"

namespace statwisp
{

// Interface placeholder; replaced by the WinHTTP worker implementation.
UpdateService::~UpdateService()
{
    Stop();
}

void UpdateService::Start(HWND, UINT, bool, std::filesystem::path)
{
}

void UpdateService::Stop() noexcept
{
}

void UpdateService::SetAutomaticChecks(bool)
{
}

void UpdateService::CheckNow()
{
}

void UpdateService::InstallUpdate()
{
}

UpdateState UpdateService::State() const
{
    std::scoped_lock lock(mutex_);
    return state_;
}

bool UpdateService::DisabledByPolicy() noexcept
{
    return false;
}

bool UpdateService::IsInstalledEdition() noexcept
{
    return false;
}

void UpdateService::CleanupAfterUpdate(const std::filesystem::path &) noexcept
{
}

} // namespace statwisp
