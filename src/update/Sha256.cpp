#include "update/Sha256.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>

namespace statwisp
{

Sha256::Sha256() noexcept
{
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
    {
        failed_ = true;
        return;
    }
    algorithm_ = algorithm;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0)))
    {
        failed_ = true;
        return;
    }
    hash_ = hash;
}

Sha256::~Sha256()
{
    if (hash_)
    {
        BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(hash_));
    }
    if (algorithm_)
    {
        BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
    }
}

bool Sha256::Update(const void *data, std::size_t size) noexcept
{
    auto *bytes = static_cast<const UCHAR *>(data);
    while (!failed_ && size != 0)
    {
        const auto chunk = static_cast<ULONG>(size > 0x40000000 ? 0x40000000 : size);
        if (!BCRYPT_SUCCESS(BCryptHashData(static_cast<BCRYPT_HASH_HANDLE>(hash_), const_cast<UCHAR *>(bytes), chunk, 0)))
        {
            failed_ = true;
        }
        bytes += chunk;
        size -= chunk;
    }
    return !failed_;
}

std::optional<Sha256Digest> Sha256::Finish() noexcept
{
    Sha256Digest digest{};
    if (failed_ || !BCRYPT_SUCCESS(BCryptFinishHash(static_cast<BCRYPT_HASH_HANDLE>(hash_), digest.data(),
                                                    static_cast<ULONG>(digest.size()), 0)))
    {
        failed_ = true;
        return std::nullopt;
    }
    failed_ = true; // A finished CNG hash cannot be reused.
    return digest;
}

std::optional<Sha256Digest> Sha256File(const std::filesystem::path &path) noexcept
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }
    Sha256 hash;
    std::array<std::uint8_t, 64 * 1024> buffer{};
    bool ok = true;
    for (;;)
    {
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
        {
            ok = false;
            break;
        }
        if (read == 0)
        {
            break;
        }
        if (!hash.Update(buffer.data(), read))
        {
            ok = false;
            break;
        }
    }
    CloseHandle(file);
    return ok ? hash.Finish() : std::nullopt;
}

} // namespace statwisp
