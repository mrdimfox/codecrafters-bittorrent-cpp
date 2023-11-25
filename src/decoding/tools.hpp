#pragma once

#include <charconv>
#include <optional>
#include <string_view>

namespace decoding {

inline auto to_integer(std::string_view s) -> std::optional<long long int>
{
    long long int value{};
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);

    if (result.ec != std::errc{} or result.ptr != s.end()) {
        return std::nullopt;
    }

    return value;
};

}  // namespace decoding
