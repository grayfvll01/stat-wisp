#include "update/Sha256.h"

namespace statwisp
{

// Interface placeholder; replaced by the CNG implementation.
Sha256::Sha256() noexcept : failed_(true)
{
}

Sha256::~Sha256() = default;

bool Sha256::Update(const void *, std::size_t) noexcept
{
    return false;
}

std::optional<Sha256Digest> Sha256::Finish() noexcept
{
    return std::nullopt;
}

std::optional<Sha256Digest> Sha256File(const std::filesystem::path &) noexcept
{
    return std::nullopt;
}

} // namespace statwisp
