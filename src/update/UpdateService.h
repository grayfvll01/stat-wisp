#pragma once

#include "core/SemanticVersion.h"
#include "update/UpdateManifest.h"

#include <Windows.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

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
    // downloads in workDirectory. Call once at startup; after a relaunch, pass a
    // short retry period because the previous process may still be exiting.
    static void CleanupAfterUpdate(const std::filesystem::path &workDirectory,
                                   std::chrono::milliseconds retryFor = std::chrono::milliseconds(0)) noexcept;

  private:
    enum class Request : std::uint8_t
    {
        None,
        Check,
        Install,
    };
    using Sink = std::function<bool(const std::uint8_t *, DWORD)>;

    void Run(std::stop_token stopToken);
    void Check(bool userInitiated);
    void Install();
    [[nodiscard]] bool Download(const UpdateAsset &asset, const std::filesystem::path &destination,
                                std::wstring &error);
    [[nodiscard]] bool HttpGet(const std::wstring &url, std::uint64_t maxBytes, const Sink &sink, std::wstring &error);
    void Publish(const std::function<void(UpdateState &)> &change);
    [[nodiscard]] bool Cancelled() const;

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    UpdateState state_;
    std::optional<UpdateManifest> manifest_;
    Request request_{Request::None};
    bool requestUserInitiated_{};
    bool automaticChecks_{};
    bool cancelled_{};
    HWND notifyWindow_{};
    UINT notifyMessage_{};
    void *activeRequest_{}; // HINTERNET of the request in flight; closed by Stop() to cancel it.
    std::filesystem::path workDirectory_;
    std::jthread worker_;
};

} // namespace statwisp
