#pragma once

#include <optional>
#include <string_view>

#include "bencode/types.hpp"

namespace bencode {

auto decode_bencoded_value(std::string_view encoded_value)
  -> std::optional<DecodedValue>;

namespace internal {

auto detect_bencoded_value_type(std::string_view bencoded_value)
  -> EncodedValueType;

auto is_encoded_string_ahead(std::string_view encoded_value) -> bool;
auto decode_string(std::string_view) -> std::optional<DecodedValue>;

auto is_encoded_integer_ahead(std::string_view encoded_value) -> bool;
auto decode_integer(std::string_view) -> std::optional<DecodedValue>;

auto is_encoded_list_ahead(std::string_view) -> bool;
auto decode_bencoded_list(std::string_view) -> std::optional<DecodedValue>;

auto is_encoded_dict_ahead(std::string_view) -> bool;
auto decode_bencoded_dict(std::string_view) -> std::optional<DecodedValue>;

}  // namespace internal

}  // namespace bencode
