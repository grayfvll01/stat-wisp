#include "core/SemanticVersion.h"

#include <charconv>

namespace statwisp
{

std::optional<SemanticVersion> SemanticVersion::Parse(std::string_view text) noexcept
{
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
    {
        text.remove_prefix(1);
    }
    SemanticVersion version;
    unsigned *fields[]{&version.major, &version.minor, &version.patch};
    for (std::size_t index = 0; index < 3; ++index)
    {
        const auto separator = index == 2 ? std::string_view::npos : text.find('.');
        const auto token = text.substr(0, separator);
        if (token.empty())
        {
            return std::nullopt;
        }
        const auto result = std::from_chars(token.data(), token.data() + token.size(), *fields[index]);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
        {
            return std::nullopt;
        }
        if (separator == std::string_view::npos)
        {
            if (index != 2)
            {
                return std::nullopt;
            }
        }
        else
        {
            text.remove_prefix(separator + 1);
        }
    }
    return version;
}

std::string SemanticVersion::ToString() const
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

} // namespace statwisp
