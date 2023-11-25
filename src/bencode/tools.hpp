#pragma once

#include <charconv>
#include <optional>
#include <string_view>

#include "bencode/types.hpp"

namespace bencode {

inline auto to_integer(std::string_view s) -> std::optional<Integer>
{
    Integer value{};
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);

    if (result.ec != std::errc{} or result.ptr != s.end()) {
        return std::nullopt;
    }

    return value;
};

}  // namespace bencode
