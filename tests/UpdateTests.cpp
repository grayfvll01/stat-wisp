#include "update/Sha256.h"
#include "update/UpdateManifest.h"
#include "update/UpdateService.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

void Check(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

constexpr std::string_view kHashA = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
constexpr std::string_view kHashB = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

std::string ValidManifest()
{
    return "format=1\n"
           "version=1.2.3\n"
           "installer=stat-wisp-1.2.3-setup.exe\n"
           "installer_sha256=" +
           std::string(kHashA) +
           "\n"
           "installer_size=2048\n"
           "portable=stat-wisp.exe\n"
           "portable_sha256=" +
           std::string(kHashB) + "\nportable_size=1024\nfuture_key=ignored\n";
}

void TestManifestParsing()
{
    const auto manifest = statwisp::ParseUpdateManifest(ValidManifest());
    Check(manifest.has_value(), "valid manifest was rejected");
    Check(manifest->version == statwisp::SemanticVersion{1, 2, 3}, "manifest version was misread");
    Check(manifest->installer.fileName == L"stat-wisp-1.2.3-setup.exe" && manifest->installer.size == 2048,
          "installer asset was misread");
    Check(statwisp::Sha256ToHex(manifest->installer.sha256) == kHashA, "installer hash was misread");
    Check(manifest->portable.fileName == L"stat-wisp.exe" && manifest->portable.size == 1024,
          "portable asset was misread");

    auto crlf = ValidManifest();
    for (std::size_t position = 0; (position = crlf.find('\n', position)) != std::string::npos; position += 2)
    {
        crlf.insert(position, "\r");
    }
    Check(statwisp::ParseUpdateManifest("\xEF\xBB\xBF" + crlf).has_value(), "CRLF or BOM manifest was rejected");

    const auto replace = [](std::string text, std::string_view from, std::string_view to) {
        text.replace(text.find(from), from.size(), to);
        return text;
    };
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "format=1", "format=2")), "unknown format accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "version=1.2.3", "version=1.2")),
          "incomplete version accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "version=1.2.3", "version=v1.2.3")),
          "tag-style version accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "stat-wisp.exe", "..\\stat-wisp.exe")),
          "path traversal accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "stat-wisp.exe", "a/b.exe")),
          "directory separator accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "portable_size=1024", "portable_size=0")),
          "zero size accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "portable_size=1024", "portable_size=999999999999")),
          "oversized asset accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), std::string(kHashB), std::string(kHashB.substr(1)))),
          "short hash accepted");
    Check(!statwisp::ParseUpdateManifest(replace(ValidManifest(), "installer_size=2048\n", "")),
          "missing field accepted");
    Check(!statwisp::ParseUpdateManifest(ValidManifest() + std::string(statwisp::kMaxUpdateManifestBytes, '#')),
          "oversized manifest accepted");
    Check(!statwisp::ParseUpdateManifest("<html>Not Found</html>"), "HTML error page accepted");
}

void TestAssetNames()
{
    Check(statwisp::IsSafeAssetFileName(L"stat-wisp-1.0.0-setup.exe"), "normal asset name rejected");
    Check(!statwisp::IsSafeAssetFileName(L""), "empty name accepted");
    Check(!statwisp::IsSafeAssetFileName(L".hidden"), "dot-prefixed name accepted");
    Check(!statwisp::IsSafeAssetFileName(L"a..b"), "double dot accepted");
    Check(!statwisp::IsSafeAssetFileName(L"C:evil.exe"), "drive-relative name accepted");
    Check(!statwisp::IsSafeAssetFileName(L"nul.exe"), "device name accepted");
    Check(!statwisp::IsSafeAssetFileName(L"COM1"), "device name accepted");
    Check(!statwisp::IsSafeAssetFileName(std::wstring(129, L'a')), "overlong name accepted");
}

void TestVersionOrdering()
{
    using statwisp::SemanticVersion;
    Check(SemanticVersion{1, 0, 1} > SemanticVersion{1, 0, 0}, "patch ordering failed");
    Check(SemanticVersion{1, 10, 0} > SemanticVersion{1, 9, 9}, "numeric minor ordering failed");
    Check(SemanticVersion{2, 0, 0} > SemanticVersion{1, 99, 99}, "major ordering failed");
    Check(!(SemanticVersion{1, 0, 0} > SemanticVersion{1, 0, 0}), "equal version treated as newer");
    Check(SemanticVersion{1, 2, 3}.ToWString() == L"1.2.3", "wide version formatting failed");
}

void TestSha256()
{
    statwisp::Sha256 hash;
    Check(hash.Update("a", 1) && hash.Update("bc", 2), "hash update failed");
    const auto digest = hash.Finish();
    Check(digest && statwisp::Sha256ToHex(*digest) == kHashA, "SHA-256 of abc is wrong");

    const auto path = std::filesystem::temp_directory_path() /
                      (L"stat-wisp-hash-" + std::to_wstring(GetCurrentProcessId()) + L".bin");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    }
    const auto empty = statwisp::Sha256File(path);
    std::filesystem::remove(path);
    Check(empty && statwisp::Sha256ToHex(*empty) == kHashB, "SHA-256 of an empty file is wrong");
    Check(!statwisp::Sha256File(path).has_value(), "missing file produced a hash");
    Check(statwisp::ParseSha256Hex(kHashA) == *digest, "hex parsing does not round trip");
}

void TestUrls()
{
    SetEnvironmentVariableW(L"STAT_WISP_UPDATE_BASE_URL", nullptr);
    Check(statwisp::UpdateManifestUrl() == statwisp::kLatestManifestUrl, "default manifest URL is wrong");
    Check(statwisp::UpdateAssetUrl({1, 2, 3}, L"stat-wisp.exe") ==
              L"https://github.com/grayfvll01/stat-wisp/releases/download/v1.2.3/stat-wisp.exe",
          "asset URL is wrong");
    SetEnvironmentVariableW(L"STAT_WISP_UPDATE_BASE_URL", L"http://127.0.0.1:8080/");
    Check(statwisp::UpdateManifestUrl() == L"http://127.0.0.1:8080/stat-wisp-update.ini", "local override ignored");
    SetEnvironmentVariableW(L"STAT_WISP_UPDATE_BASE_URL", L"http://example.com");
    Check(statwisp::UpdateManifestUrl() == statwisp::kLatestManifestUrl, "remote plain-HTTP override accepted");
    SetEnvironmentVariableW(L"STAT_WISP_UPDATE_BASE_URL", nullptr);
}

void TestServiceLifecycle()
{
    // Start and Stop must be prompt and must not touch the network before the
    // first scheduled check.
    statwisp::UpdateService service;
    const auto directory = std::filesystem::temp_directory_path() /
                           (L"stat-wisp-update-test-" + std::to_wstring(GetCurrentProcessId()));
    const auto started = GetTickCount64();
    service.Start(nullptr, 0, true, directory);
    service.InstallUpdate(); // Nothing is available: ignored.
    service.Stop();
    Check(GetTickCount64() - started < 2000, "update service did not stop promptly");
    const auto status = service.State().status;
    Check(status == statwisp::UpdateStatus::Idle || status == statwisp::UpdateStatus::Disabled,
          "update service changed state without a request");
    statwisp::UpdateService::CleanupAfterUpdate(directory);
}

} // namespace

int main()
{
    try
    {
        TestManifestParsing();
        TestAssetNames();
        TestVersionOrdering();
        TestSha256();
        TestUrls();
        TestServiceLifecycle();
        std::cout << "stat-wisp update tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "stat-wisp update test failure: " << error.what() << '\n';
        return 1;
    }
}
