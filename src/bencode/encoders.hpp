#pragma once

#include <optional>
#include <string>

#include "bencode/types.hpp"

namespace bencode {

auto encode(bencode::Json) -> std::optional<std::string>;

namespace internal {

auto encode_integer(Json) -> std::string;
auto encode_string(Json) -> std::string;
auto encode_binary(Json) -> std::string;
auto encode_dict(Json) -> std::optional<std::string>;
auto encode_list(Json) -> std::optional<std::string>;

}

}  // namespace bencode
