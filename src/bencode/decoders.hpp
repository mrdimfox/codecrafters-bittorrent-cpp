#pragma once

#include <optional>
#include <string_view>

#include "bencode/types.hpp"

namespace bencode {

auto decode_bencoded_value(std::string_view encoded_value)
  -> std::optional<DecodedValue>;

namespace internal {

auto extract_bencoded_value(std::string_view) -> EncodedValue;

auto extract_bencoded_string(std::string_view encoded_value)
  -> std::optional<std::string_view>;

Json decode_string(std::string_view encoded_string);

auto extract_bencoded_integer(std::string_view encoded_value)
  -> std::optional<std::string_view>;

Json decode_integer(std::string_view encoded_integer);

auto is_encoded_list_ahead(std::string_view) -> bool;

auto decode_bencoded_list(std::string_view) -> std::optional<DecodedValue>;

auto is_encoded_dict_ahead(std::string_view) -> bool;

auto decode_bencoded_dict(std::string_view) -> std::optional<DecodedValue>;

}  // namespace internal

}  // namespace bencode
