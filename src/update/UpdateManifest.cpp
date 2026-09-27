#include "update/UpdateManifest.h"

namespace statwisp
{

// Interface placeholder; replaced by the validating parser.
std::optional<UpdateManifest> ParseUpdateManifest(std::string_view)
{
    return std::nullopt;
}

bool IsSafeAssetFileName(std::wstring_view) noexcept
{
    return false;
}

std::optional<Sha256Digest> ParseSha256Hex(std::string_view) noexcept
{
    return std::nullopt;
}

std::string Sha256ToHex(const Sha256Digest &)
{
    return {};
}

std::wstring UpdateManifestUrl()
{
    return std::wstring(kLatestManifestUrl);
}

std::wstring UpdateAssetUrl(const SemanticVersion &version, std::wstring_view fileName)
{
    return std::wstring(kRepositoryUrl) + L"/releases/download/v" + version.ToWString() + L"/" + std::wstring(fileName);
}

std::wstring ReleasePageUrl(const SemanticVersion &version)
{
    return std::wstring(kRepositoryUrl) + L"/releases/tag/v" + version.ToWString();
}

} // namespace statwisp
