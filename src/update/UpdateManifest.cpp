#include "update/UpdateManifest.h"

#include <Windows.h>

#include <array>
#include <charconv>

namespace statwisp
{
namespace
{

std::string_view Trim(std::string_view text) noexcept
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
    {
        text.remove_suffix(1);
    }
    return text;
}

std::optional<std::uint64_t> ParseSize(std::string_view text) noexcept
{
    std::uint64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 ||
        value > kMaxUpdateAssetBytes)
    {
        return std::nullopt;
    }
    return value;
}

std::optional<std::wstring> ParseFileName(std::string_view text)
{
    std::wstring name(text.begin(), text.end());
    if (!IsSafeAssetFileName(name))
    {
        return std::nullopt;
    }
    return name;
}

std::wstring EnvironmentBaseUrl()
{
    std::array<wchar_t, 2048> buffer{};
    const auto length =
        GetEnvironmentVariableW(L"STAT_WISP_UPDATE_BASE_URL", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
    {
        return {};
    }
    std::wstring value(buffer.data(), length);
    while (!value.empty() && value.back() == L'/')
    {
        value.pop_back();
    }
    const auto startsWith = [&value](std::wstring_view prefix) {
        return value.size() >= prefix.size() &&
               CompareStringOrdinal(value.data(), static_cast<int>(prefix.size()), prefix.data(),
                                    static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL;
    };
    // Plain HTTP is accepted only for a local test server.
    if (startsWith(L"https://") || startsWith(L"http://127.0.0.1:") || startsWith(L"http://localhost:"))
    {
        return value;
    }
    return {};
}

} // namespace

bool IsSafeAssetFileName(std::wstring_view name) noexcept
{
    if (name.empty() || name.size() > 128 || name.front() == L'.' || name.find(L"..") != std::wstring_view::npos)
    {
        return false;
    }
    for (const auto character : name)
    {
        const bool allowed = (character >= L'a' && character <= L'z') || (character >= L'A' && character <= L'Z') ||
                             (character >= L'0' && character <= L'9') || character == L'.' || character == L'_' ||
                             character == L'-';
        if (!allowed)
        {
            return false;
        }
    }
    // Reject DOS device names such as CON or NUL.txt.
    const auto stem = name.substr(0, name.find(L'.'));
    constexpr std::array<std::wstring_view, 22> reserved{L"CON",  L"PRN",  L"AUX",  L"NUL",  L"COM1", L"COM2",
                                                         L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8",
                                                         L"COM9", L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5",
                                                         L"LPT6", L"LPT7", L"LPT8", L"LPT9"};
    for (const auto device : reserved)
    {
        if (CompareStringOrdinal(stem.data(), static_cast<int>(stem.size()), device.data(),
                                 static_cast<int>(device.size()), TRUE) == CSTR_EQUAL)
        {
            return false;
        }
    }
    return true;
}

std::optional<Sha256Digest> ParseSha256Hex(std::string_view hex) noexcept
{
    if (hex.size() != 64)
    {
        return std::nullopt;
    }
    const auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return character - 'a' + 10;
        if (character >= 'A' && character <= 'F')
            return character - 'A' + 10;
        return -1;
    };
    Sha256Digest digest{};
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        const auto high = nibble(hex[index * 2]);
        const auto low = nibble(hex[index * 2 + 1]);
        if (high < 0 || low < 0)
        {
            return std::nullopt;
        }
        digest[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return digest;
}

std::string Sha256ToHex(const Sha256Digest &digest)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(digest.size() * 2);
    for (const auto byte : digest)
    {
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 0x0F]);
    }
    return hex;
}

std::optional<UpdateManifest> ParseUpdateManifest(std::string_view text)
{
    if (text.size() > kMaxUpdateManifestBytes)
    {
        return std::nullopt;
    }
    if (text.starts_with("\xEF\xBB\xBF"))
    {
        text.remove_prefix(3);
    }

    std::optional<std::uint64_t> format;
    std::optional<SemanticVersion> version;
    std::optional<std::wstring> installer, portable;
    std::optional<Sha256Digest> installerHash, portableHash;
    std::optional<std::uint64_t> installerSize, portableSize;

    std::size_t lineStart = 0;
    while (lineStart < text.size())
    {
        const auto lineEnd = text.find('\n', lineStart);
        const auto line =
            Trim(text.substr(lineStart, lineEnd == std::string_view::npos ? text.size() - lineStart : lineEnd - lineStart));
        lineStart = lineEnd == std::string_view::npos ? text.size() : lineEnd + 1;
        const auto separator = line.find('=');
        if (line.empty() || line.front() == '#' || line.front() == ';' || separator == std::string_view::npos)
        {
            continue;
        }
        const auto key = Trim(line.substr(0, separator));
        const auto value = Trim(line.substr(separator + 1));
        if (key == "format")
        {
            std::uint64_t parsed{};
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec == std::errc{} && result.ptr == value.data() + value.size())
            {
                format = parsed;
            }
        }
        else if (key == "version")
        {
            if (value.starts_with('v') || value.starts_with('V'))
            {
                return std::nullopt;
            }
            version = SemanticVersion::Parse(value);
        }
        else if (key == "installer")
        {
            installer = ParseFileName(value);
        }
        else if (key == "installer_sha256")
        {
            installerHash = ParseSha256Hex(value);
        }
        else if (key == "installer_size")
        {
            installerSize = ParseSize(value);
        }
        else if (key == "portable")
        {
            portable = ParseFileName(value);
        }
        else if (key == "portable_sha256")
        {
            portableHash = ParseSha256Hex(value);
        }
        else if (key == "portable_size")
        {
            portableSize = ParseSize(value);
        }
    }

    if (format != kUpdateManifestFormat || !version || !installer || !installerHash || !installerSize || !portable ||
        !portableHash || !portableSize)
    {
        return std::nullopt;
    }
    return UpdateManifest{*version, {*installer, *installerHash, *installerSize}, {*portable, *portableHash, *portableSize}};
}

std::wstring UpdateManifestUrl()
{
    if (auto base = EnvironmentBaseUrl(); !base.empty())
    {
        return base + L"/" + std::wstring(kUpdateManifestName);
    }
    return std::wstring(kLatestManifestUrl);
}

std::wstring UpdateAssetUrl(const SemanticVersion &version, std::wstring_view fileName)
{
    if (auto base = EnvironmentBaseUrl(); !base.empty())
    {
        return base + L"/" + std::wstring(fileName);
    }
    return std::wstring(kRepositoryUrl) + L"/releases/download/v" + version.ToWString() + L"/" + std::wstring(fileName);
}

std::wstring ReleasePageUrl(const SemanticVersion &version)
{
    return std::wstring(kRepositoryUrl) + L"/releases/tag/v" + version.ToWString();
}

} // namespace statwisp
