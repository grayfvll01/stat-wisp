#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace statwisp
{

struct SemanticVersion
{
    unsigned major{};
    unsigned minor{};
    unsigned patch{};

    [[nodiscard]] static std::optional<SemanticVersion> Parse(std::string_view text) noexcept;
    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] std::wstring ToWString() const;
    friend bool operator==(const SemanticVersion &, const SemanticVersion &) = default;
    friend auto operator<=>(const SemanticVersion &, const SemanticVersion &) = default;
};

} // namespace statwisp
