#pragma once

#include "core/SemanticVersion.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace statwisp
{

using Sha256Digest = std::array<std::uint8_t, 32>;

// Published as the `stat-wisp-update.ini` asset of every release. GitHub serves
// the newest published release's copy from kLatestManifestUrl.
//
//   format=1
//   version=1.2.3
//   installer=stat-wisp-1.2.3-setup.exe
//   installer_sha256=<64 lowercase hex digits>
//   installer_size=<bytes>
//   portable=stat-wisp.exe
//   portable_sha256=<64 lowercase hex digits>
//   portable_size=<bytes>
//
// Unknown keys are ignored so later releases can add fields.
inline constexpr std::uint32_t kUpdateManifestFormat = 1;
inline constexpr std::size_t kMaxUpdateManifestBytes = 16 * 1024;
inline constexpr std::uint64_t kMaxUpdateAssetBytes = 64ULL * 1024ULL * 1024ULL;
inline constexpr std::wstring_view kUpdateManifestName = L"stat-wisp-update.ini";
inline constexpr std::wstring_view kRepositoryUrl = L"https://github.com/grayfvll01/stat-wisp";
inline constexpr std::wstring_view kLatestManifestUrl =
    L"https://github.com/grayfvll01/stat-wisp/releases/latest/download/stat-wisp-update.ini";

struct UpdateAsset
{
    std::wstring fileName;
    Sha256Digest sha256{};
    std::uint64_t size{};
};

struct UpdateManifest
{
    SemanticVersion version;
    UpdateAsset installer;
    UpdateAsset portable;
};

// Returns nullopt unless every required field is present and valid: a known
// format, a MAJOR.MINOR.PATCH version, safe asset file names, 64-hex-digit
// SHA-256 values, and sizes in 1..kMaxUpdateAssetBytes. Input over
// kMaxUpdateManifestBytes is rejected.
[[nodiscard]] std::optional<UpdateManifest> ParseUpdateManifest(std::string_view text);

// Only [A-Za-z0-9._-], 1..128 characters, not starting with '.', no "..".
[[nodiscard]] bool IsSafeAssetFileName(std::wstring_view name) noexcept;
[[nodiscard]] std::optional<Sha256Digest> ParseSha256Hex(std::string_view hex) noexcept;
[[nodiscard]] std::string Sha256ToHex(const Sha256Digest &digest);

// STAT_WISP_UPDATE_BASE_URL (tests only) replaces the release location when it
// starts with https:// or with http://127.0.0.1 or http://localhost. Otherwise
// these return the GitHub URLs for this repository.
[[nodiscard]] std::wstring UpdateManifestUrl();
[[nodiscard]] std::wstring UpdateAssetUrl(const SemanticVersion &version, std::wstring_view fileName);
[[nodiscard]] std::wstring ReleasePageUrl(const SemanticVersion &version);

} // namespace statwisp
