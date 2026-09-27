#include "update/UpdateService.h"

#include "core/Log.h"
#include "update/Sha256.h"
#include "version.h"

#include <Objbase.h>
#include <shellapi.h>
#include <winhttp.h>

#include <array>
#include <string>
#include <vector>

namespace statwisp
{
namespace
{

constexpr wchar_t kPolicyKey[] = L"SOFTWARE\\Policies\\StatWisp";
constexpr wchar_t kUserAgent[] = L"StatWisp/" STAT_WISP_VERSION_WSTRING;

class InternetHandle final
{
  public:
    explicit InternetHandle(HINTERNET handle) noexcept : handle_(handle)
    {
    }
    ~InternetHandle()
    {
        if (handle_)
        {
            WinHttpCloseHandle(handle_);
        }
    }
    InternetHandle(const InternetHandle &) = delete;
    InternetHandle &operator=(const InternetHandle &) = delete;
    [[nodiscard]] HINTERNET Get() const noexcept
    {
        return handle_;
    }

  private:
    HINTERNET handle_{};
};

std::filesystem::path ExecutablePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < buffer.size())
        {
            buffer.resize(length);
            return buffer;
        }
        if (buffer.size() >= 32768)
        {
            return {};
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path WithSuffix(const std::filesystem::path &path, const wchar_t *suffix)
{
    auto result = path;
    result += suffix;
    return result;
}

std::optional<SemanticVersion> CurrentVersion() noexcept
{
    return SemanticVersion::Parse(STAT_WISP_VERSION_STRING);
}

bool PolicyDisables(HKEY root) noexcept
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    return RegGetValueW(root, kPolicyKey, L"DisableUpdates", RRF_RT_REG_DWORD, nullptr, &value, &size) ==
               ERROR_SUCCESS &&
           value != 0;
}

bool ShellOpen(const std::wstring &file, const wchar_t *parameters) noexcept
{
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = file.c_str();
    info.lpParameters = parameters;
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) != FALSE;
}

bool SameDigest(const std::filesystem::path &path, const Sha256Digest &expected) noexcept
{
    const auto actual = Sha256File(path);
    return actual && *actual == expected;
}

} // namespace

UpdateService::~UpdateService()
{
    Stop();
}

void UpdateService::Start(HWND notifyWindow, UINT notifyMessage, bool automaticChecks,
                          std::filesystem::path workDirectory)
{
    Stop();
    {
        std::scoped_lock lock(mutex_);
        notifyWindow_ = notifyWindow;
        notifyMessage_ = notifyMessage;
        automaticChecks_ = automaticChecks;
        workDirectory_ = std::move(workDirectory);
        cancelled_ = false;
        request_ = Request::None;
        if (DisabledByPolicy())
        {
            state_.status = UpdateStatus::Disabled;
            ++state_.sequence;
            return;
        }
    }
    worker_ = std::jthread([this](std::stop_token token) { Run(token); });
}

void UpdateService::Stop() noexcept
{
    if (!worker_.joinable())
    {
        return;
    }
    worker_.request_stop();
    {
        std::scoped_lock lock(mutex_);
        cancelled_ = true;
        if (activeRequest_)
        {
            // Closing the handle from another thread cancels a blocking WinHTTP call.
            WinHttpCloseHandle(static_cast<HINTERNET>(activeRequest_));
            activeRequest_ = nullptr;
        }
    }
    wake_.notify_all();
    if (WaitForSingleObject(worker_.native_handle(), 3000) != WAIT_OBJECT_0)
    {
        // Same bounded-exit rule as the monitor: settings are already saved.
        TerminateProcess(GetCurrentProcess(), ERROR_TIMEOUT);
    }
    worker_.join();
}

void UpdateService::SetAutomaticChecks(bool enabled)
{
    {
        std::scoped_lock lock(mutex_);
        automaticChecks_ = enabled;
    }
    wake_.notify_all();
}

void UpdateService::CheckNow()
{
    {
        std::scoped_lock lock(mutex_);
        const auto status = state_.status;
        if (!worker_.joinable() || status == UpdateStatus::Disabled || status == UpdateStatus::Checking ||
            status == UpdateStatus::Downloading || status == UpdateStatus::Installing || request_ != Request::None)
        {
            return;
        }
        request_ = Request::Check;
        requestUserInitiated_ = true;
    }
    wake_.notify_all();
}

void UpdateService::InstallUpdate()
{
    {
        std::scoped_lock lock(mutex_);
        const auto status = state_.status;
        if (!worker_.joinable() || !manifest_ || request_ != Request::None ||
            (status != UpdateStatus::Available && status != UpdateStatus::Failed))
        {
            return;
        }
        request_ = Request::Install;
        requestUserInitiated_ = true;
    }
    wake_.notify_all();
}

UpdateState UpdateService::State() const
{
    std::scoped_lock lock(mutex_);
    return state_;
}

bool UpdateService::DisabledByPolicy() noexcept
{
    return PolicyDisables(HKEY_LOCAL_MACHINE) || PolicyDisables(HKEY_CURRENT_USER);
}

bool UpdateService::IsInstalledEdition() noexcept
{
    try
    {
        const auto executable = ExecutablePath();
        std::error_code error;
        return !executable.empty() && std::filesystem::is_regular_file(executable.parent_path() / L"unins000.exe", error);
    }
    catch (...)
    {
        return false;
    }
}

void UpdateService::CleanupAfterUpdate(const std::filesystem::path &workDirectory,
                                       std::chrono::milliseconds retryFor) noexcept
{
    try
    {
        const auto executable = ExecutablePath();
        const auto deadline = GetTickCount64() + static_cast<ULONGLONG>(retryFor.count());
        for (;;)
        {
            std::error_code error;
            bool clean = true;
            if (!executable.empty())
            {
                for (const auto *suffix : {L".old", L".new"})
                {
                    const auto leftover = WithSuffix(executable, suffix);
                    std::filesystem::remove(leftover, error);
                    clean = clean && !std::filesystem::exists(leftover, error);
                }
            }
            // A just-finished installer may still be running from here.
            if (!workDirectory.empty())
            {
                std::filesystem::remove_all(workDirectory, error);
                clean = clean && !std::filesystem::exists(workDirectory, error);
            }
            if (clean || GetTickCount64() >= deadline)
            {
                return;
            }
            Sleep(250);
        }
    }
    catch (...)
    {
    }
}

void UpdateService::Publish(const std::function<void(UpdateState &)> &change)
{
    HWND window{};
    UINT message{};
    {
        std::scoped_lock lock(mutex_);
        change(state_);
        ++state_.sequence;
        window = notifyWindow_;
        message = notifyMessage_;
    }
    if (window && message)
    {
        PostMessageW(window, message, 0, 0);
    }
}

bool UpdateService::Cancelled() const
{
    std::scoped_lock lock(mutex_);
    return cancelled_;
}

void UpdateService::Run(std::stop_token stopToken)
{
    // ShellExecuteEx needs COM on the calling thread.
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    auto nextAutomatic = std::chrono::steady_clock::now() + kFirstUpdateCheckDelay;
    while (!stopToken.stop_requested())
    {
        Request request = Request::None;
        bool userInitiated = false;
        {
            std::unique_lock lock(mutex_);
            const auto now = std::chrono::steady_clock::now();
            const auto wait = automaticChecks_ && nextAutomatic > now
                                  ? std::chrono::duration_cast<std::chrono::milliseconds>(nextAutomatic - now)
                                  : std::chrono::milliseconds(automaticChecks_ ? 0 : 3'600'000);
            wake_.wait_for(lock, stopToken, wait, [this] { return request_ != Request::None; });
            if (stopToken.stop_requested())
            {
                break;
            }
            request = request_;
            userInitiated = requestUserInitiated_;
            request_ = Request::None;
            requestUserInitiated_ = false;
            if (request == Request::None && automaticChecks_ && std::chrono::steady_clock::now() >= nextAutomatic)
            {
                request = Request::Check;
            }
        }
        try
        {
            if (request == Request::Check)
            {
                Check(userInitiated);
                nextAutomatic = std::chrono::steady_clock::now() + kUpdateCheckInterval;
            }
            else if (request == Request::Install)
            {
                Install();
            }
        }
        catch (...)
        {
            LogError(L"Update operation failed unexpectedly.");
            Publish([userInitiated](UpdateState &state) {
                state.status = UpdateStatus::Failed;
                state.userInitiated = userInitiated;
                state.message = L"The update could not be completed.";
            });
        }
    }
    if (SUCCEEDED(com))
    {
        CoUninitialize();
    }
}

void UpdateService::Check(bool userInitiated)
{
    if (DisabledByPolicy())
    {
        Publish([](UpdateState &state) { state.status = UpdateStatus::Disabled; });
        return;
    }
    Publish([userInitiated](UpdateState &state) {
        state.status = UpdateStatus::Checking;
        state.userInitiated = userInitiated;
        state.message.clear();
    });

    std::string text;
    std::wstring error;
    const bool received = HttpGet(UpdateManifestUrl(), kMaxUpdateManifestBytes,
                                  [&text](const std::uint8_t *data, DWORD size) {
                                      text.append(reinterpret_cast<const char *>(data), size);
                                      return true;
                                  },
                                  error);
    if (Cancelled())
    {
        return;
    }
    const auto manifest = received ? ParseUpdateManifest(text) : std::nullopt;
    const auto current = CurrentVersion();
    if (!manifest || !current)
    {
        LogWarning(L"Update check failed: " + (received ? std::wstring(L"invalid release manifest") : error));
        Publish([userInitiated](UpdateState &state) {
            state.status = UpdateStatus::Failed;
            state.userInitiated = userInitiated;
            state.message = L"Could not check for updates. Try again later.";
        });
        return;
    }

    if (manifest->version > *current)
    {
        LogInfo(L"Update available: " + manifest->version.ToWString());
        {
            std::scoped_lock lock(mutex_);
            manifest_ = manifest;
        }
        Publish([&manifest, userInitiated](UpdateState &state) {
            state.status = UpdateStatus::Available;
            state.availableVersion = manifest->version;
            state.userInitiated = userInitiated;
            state.message.clear();
        });
        return;
    }
    LogInfo(L"Update check: " + current->ToWString() + L" is current.");
    {
        std::scoped_lock lock(mutex_);
        manifest_.reset();
    }
    Publish([userInitiated](UpdateState &state) {
        state.status = UpdateStatus::UpToDate;
        state.availableVersion.reset();
        state.userInitiated = userInitiated;
        state.message.clear();
    });
}

void UpdateService::Install()
{
    std::optional<UpdateManifest> manifest;
    std::filesystem::path workDirectory;
    {
        std::scoped_lock lock(mutex_);
        manifest = manifest_;
        workDirectory = workDirectory_;
    }
    if (!manifest || workDirectory.empty() || DisabledByPolicy())
    {
        return;
    }
    const auto fail = [this](std::wstring message, const std::wstring &detail) {
        LogWarning(L"Update installation failed: " + detail);
        Publish([&message](UpdateState &state) {
            state.status = UpdateStatus::Failed;
            state.userInitiated = true;
            state.message = std::move(message);
        });
    };

    Publish([](UpdateState &state) {
        state.status = UpdateStatus::Downloading;
        state.progressPercent = 0;
        state.userInitiated = true;
        state.message.clear();
    });

    std::error_code fileError;
    std::filesystem::create_directories(workDirectory, fileError);
    const bool installed = IsInstalledEdition();
    const auto &asset = installed ? manifest->installer : manifest->portable;
    const auto downloaded = workDirectory / asset.fileName;
    std::wstring error;
    if (!Download(asset, downloaded, error))
    {
        if (!Cancelled())
        {
            fail(L"The update could not be downloaded. Try again later.", error);
        }
        return;
    }

    if (installed)
    {
        const auto installer = downloaded.wstring();
        LogInfo(L"Starting installer for " + manifest->version.ToWString());
        if (!ShellOpen(installer, L"/SILENT /SUPPRESSMSGBOXES /NORESTART /SP- /CLOSEAPPLICATIONS /RELAUNCH=1"))
        {
            fail(L"The update installer could not be started.", L"installer launch error " +
                                                                     std::to_wstring(GetLastError()));
            return;
        }
        Publish([](UpdateState &state) { state.status = UpdateStatus::Installing; });
        return;
    }

    // Portable edition: Windows allows renaming a running executable, so the
    // new file can take its place before this process exits.
    const auto executable = ExecutablePath();
    if (executable.empty())
    {
        fail(L"The update could not be installed.", L"executable path unavailable");
        return;
    }
    const auto staged = WithSuffix(executable, L".new");
    const auto previous = WithSuffix(executable, L".old");
    if (!CopyFileW(downloaded.c_str(), staged.c_str(), FALSE) || !SameDigest(staged, asset.sha256))
    {
        DeleteFileW(staged.c_str());
        // Probably a read-only folder: offer the download page instead.
        ShellOpen(ReleasePageUrl(manifest->version), nullptr);
        fail(L"Stat Wisp cannot write to its folder. The download page was opened instead.",
             L"cannot stage portable executable");
        return;
    }
    DeleteFileW(previous.c_str());
    if (!MoveFileExW(executable.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(staged.c_str());
        fail(L"The update could not be installed.", L"cannot rename running executable");
        return;
    }
    if (!MoveFileExW(staged.c_str(), executable.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        MoveFileExW(previous.c_str(), executable.c_str(), MOVEFILE_REPLACE_EXISTING);
        DeleteFileW(staged.c_str());
        fail(L"The update could not be installed.", L"cannot move new executable into place");
        return;
    }

    std::wstring commandLine = L"\"" + executable.wstring() + L"\" --relaunch";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        executable.parent_path().c_str(), &startup, &process))
    {
        fail(L"Update installed. Restart Stat Wisp to use it.", L"relaunch failed");
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    LogInfo(L"Portable update to " + manifest->version.ToWString() + L" installed; restarting.");
    Publish([](UpdateState &state) { state.status = UpdateStatus::Installing; });
}

bool UpdateService::Download(const UpdateAsset &asset, const std::filesystem::path &destination, std::wstring &error)
{
    const auto partial = WithSuffix(destination, L".partial");
    HANDLE file = CreateFileW(partial.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        error = L"cannot create download file";
        return false;
    }
    std::uint64_t written = 0;
    std::uint32_t reported = 0;
    std::optional<SemanticVersion> version;
    {
        std::scoped_lock lock(mutex_);
        if (manifest_)
        {
            version = manifest_->version;
        }
    }
    if (!version)
    {
        CloseHandle(file);
        DeleteFileW(partial.c_str());
        error = L"no update selected";
        return false;
    }
    const bool received = HttpGet(UpdateAssetUrl(*version, asset.fileName), asset.size,
                                  [&](const std::uint8_t *data, DWORD size) {
                                      DWORD done = 0;
                                      if (!WriteFile(file, data, size, &done, nullptr) || done != size)
                                      {
                                          return false;
                                      }
                                      written += size;
                                      const auto percent = static_cast<std::uint32_t>(written * 100 / asset.size);
                                      if (percent >= reported + 5)
                                      {
                                          reported = percent;
                                          Publish([percent](UpdateState &state) { state.progressPercent = percent; });
                                      }
                                      return true;
                                  },
                                  error);
    const bool flushed = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!received || !flushed)
    {
        DeleteFileW(partial.c_str());
        if (error.empty())
        {
            error = L"download failed";
        }
        return false;
    }
    if (written != asset.size || !SameDigest(partial, asset.sha256))
    {
        DeleteFileW(partial.c_str());
        error = L"downloaded file failed size or SHA-256 verification";
        return false;
    }
    if (!MoveFileExW(partial.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(partial.c_str());
        error = L"cannot finalize download";
        return false;
    }
    return true;
}

bool UpdateService::HttpGet(const std::wstring &url, std::uint64_t maxBytes, const Sink &sink, std::wstring &error)
{
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
    {
        error = L"invalid URL";
        return false;
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    if (!secure && host != L"127.0.0.1" && host != L"localhost")
    {
        error = L"refusing an insecure URL";
        return false;
    }

    InternetHandle session(
        WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.Get())
    {
        error = L"cannot open HTTP session (" + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    WinHttpSetTimeouts(session.Get(), 15000, 15000, 30000, 30000);
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(session.Get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
#endif
    {
        DWORD tls12 = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        WinHttpSetOption(session.Get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &tls12, sizeof(tls12));
    }
    InternetHandle connection(WinHttpConnect(session.Get(), host.c_str(), parts.nPort, 0));
    if (!connection.Get())
    {
        error = L"cannot connect (" + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    HINTERNET request = WinHttpOpenRequest(connection.Get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request)
    {
        error = L"cannot create request (" + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    {
        std::scoped_lock lock(mutex_);
        if (cancelled_)
        {
            WinHttpCloseHandle(request);
            error = L"cancelled";
            return false;
        }
        activeRequest_ = request;
    }
    struct RequestGuard
    {
        UpdateService &service;
        HINTERNET request;
        ~RequestGuard()
        {
            std::scoped_lock lock(service.mutex_);
            if (service.activeRequest_ == request)
            {
                WinHttpCloseHandle(request);
                service.activeRequest_ = nullptr;
            }
        }
    } guard{*this, request};

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr))
    {
        error = L"request failed (" + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) ||
        status != 200)
    {
        error = L"HTTP status " + std::to_wstring(status);
        return false;
    }

    std::vector<std::uint8_t> buffer(32 * 1024);
    std::uint64_t total = 0;
    for (;;)
    {
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
        {
            error = L"transfer failed (" + std::to_wstring(GetLastError()) + L")";
            return false;
        }
        if (read == 0)
        {
            return true;
        }
        total += read;
        if (total > maxBytes)
        {
            error = L"response is larger than expected";
            return false;
        }
        if (Cancelled() || !sink(buffer.data(), read))
        {
            error = L"transfer stopped";
            return false;
        }
    }
}

} // namespace statwisp
