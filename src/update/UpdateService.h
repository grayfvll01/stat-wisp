#pragma once

#include "core/SemanticVersion.h"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace statwisp
{

inline constexpr UINT kUpdateStateMessage = WM_APP + 4;
inline constexpr auto kFirstUpdateCheckDelay = std::chrono::seconds(90);
inline constexpr auto kUpdateCheckInterval = std::chrono::hours(24);

enum class UpdateStatus : std::uint8_t
{
    Idle,        // No check has completed yet.
    Checking,    // Downloading and validating the release manifest.
    UpToDate,    // The latest published release is not newer.
    Available,   // availableVersion is newer than this build.
    Downloading, // progressPercent is valid.
    Installing,  // The update was started; the application must exit now.
    Failed,      // message explains; a previously found availableVersion is kept.
    Disabled,    // Turned off by policy.
};

struct UpdateState
{
    UpdateStatus status{UpdateStatus::Idle};
    std::optional<SemanticVersion> availableVersion;
    std::uint32_t progressPercent{};
    bool userInitiated{};  // The operation that produced this state was requested by the user.
    std::wstring message;  // Short and user-facing; set for Failed.
    std::uint64_t sequence{}; // Increments on every state change.
};

// Checks the latest GitHub release manifest over HTTPS, and on request
// downloads the matching installer (installed edition) or executable
// (portable edition), verifies its size and SHA-256, and starts the update.
// All network and file work happens on one worker thread; the UI thread only
// calls the methods below and reads State() after kUpdateStateMessage.
class UpdateService final
{
  public:
    UpdateService() = default;
    ~UpdateService();
    UpdateService(const UpdateService &) = delete;
    UpdateService &operator=(const UpdateService &) = delete;

    // Starts the worker. Automatic checks run kFirstUpdateCheckDelay after
    // Start and then every kUpdateCheckInterval while enabled. Downloads are
    // written below workDirectory. Posts notifyMessage to notifyWindow after
    // every state change.
    void Start(HWND notifyWindow, UINT notifyMessage, bool automaticChecks, std::filesystem::path workDirectory);
    // Cancels network I/O and joins the worker within about three seconds.
    void Stop() noexcept;
    void SetAutomaticChecks(bool enabled);
    // Ignored while a check, download, or installation is in progress.
    void CheckNow();
    // Requires status Available (or Failed with availableVersion set).
    void InstallUpdate();
    [[nodiscard]] UpdateState State() const;

    // DWORD DisableUpdates != 0 under HKLM or HKCU \SOFTWARE\Policies\StatWisp.
    [[nodiscard]] static bool DisabledByPolicy() noexcept;
    // True when the executable sits beside the Inno Setup uninstaller.
    [[nodiscard]] static bool IsInstalledEdition() noexcept;
    // Best effort: removes stat-wisp.exe.old beside the executable and stale
    // downloads in workDirectory. Call once at startup.
    static void CleanupAfterUpdate(const std::filesystem::path &workDirectory) noexcept;

  private:
    mutable std::mutex mutex_;
    UpdateState state_;
};

} // namespace statwisp
