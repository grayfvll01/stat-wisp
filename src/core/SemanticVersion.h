#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace gate
{

struct SemanticVersion
{
    unsigned major{};
    unsigned minor{};
    unsigned patch{};

    [[nodiscard]] static std::optional<SemanticVersion> Parse(std::string_view text) noexcept;
    [[nodiscard]] std::string ToString() const;
    friend bool operator==(const SemanticVersion &, const SemanticVersion &) = default;
};

} // namespace gate
