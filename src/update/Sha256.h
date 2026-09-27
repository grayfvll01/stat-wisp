#pragma once

#include "update/UpdateManifest.h"

#include <cstddef>
#include <filesystem>
#include <optional>

namespace statwisp
{

// Incremental SHA-256 using Windows CNG (bcrypt.dll). Never throws; a failed
// CNG call makes Finish() return nullopt.
class Sha256 final
{
  public:
    Sha256() noexcept;
    ~Sha256();
    Sha256(const Sha256 &) = delete;
    Sha256 &operator=(const Sha256 &) = delete;

    [[nodiscard]] bool Update(const void *data, std::size_t size) noexcept;
    [[nodiscard]] std::optional<Sha256Digest> Finish() noexcept;

  private:
    void *algorithm_{};
    void *hash_{};
    bool failed_{};
};

[[nodiscard]] std::optional<Sha256Digest> Sha256File(const std::filesystem::path &path) noexcept;

} // namespace statwisp
