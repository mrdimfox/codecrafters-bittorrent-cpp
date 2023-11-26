#include "bencode/encoders.hpp"

#include <fmt/core.h>
#include <optional>
#include <sstream>

#include "bencode/consts.hpp"


namespace bencode {

auto encode(bencode::Json value) -> std::optional<std::string>
{
    using namespace internal;

    std::stringstream stream;

    if (value.is_number_integer()) {
        return encode_integer(value);
    }
    else if (value.is_string()) {
        return encode_string(value);
    }
    else if (value.is_binary()) {
        return encode_binary(value);
    }
    else if (value.is_object()) {
        return encode_dict(value);
    }
    else if (value.is_array()) {
        return encode_list(value);
    }
    else {
        return std::nullopt;
    }

    return stream.str();
}

namespace internal {

auto encode_integer(Json value) -> std::string
{
    return fmt::format(
      "{}{}{}", INTEGER_START_SYMBOL, value.dump(), END_SYMBOL
    );
}

auto encode_string(Json value) -> std::string
{
    std::string str = value;
    return fmt::format("{}{}{}", str.length(), STRING_DELIMITER_SYMBOL, str);
}

auto encode_binary(Json value) -> std::string
{
    auto bin = value.get<Json::binary_t>();
    std::string bin_str(bin.begin(), bin.end());
    return fmt::format(
      "{}{}{}", bin_str.length(), STRING_DELIMITER_SYMBOL, bin_str
    );
}

auto encode_dict(Json dict) -> std::optional<std::string>
{
    std::stringstream stream;

    stream << DICT_START_SYMBOL;

    for (auto& [key, value] : dict.items()) {
        stream << encode_string(key);

        auto encoded_value = encode(value);
        if (not encoded_value) {
            return std::nullopt;
        }

        stream << *encoded_value;
    }

    stream << END_SYMBOL;

    return stream.str();
}

auto encode_list(Json list) -> std::optional<std::string>
{
    std::stringstream stream;

    stream << LIST_START_SYMBOL;

    for (auto& [_, value] : list.items()) {
        auto encoded_value = encode(value);
        if (not encoded_value) {
            return std::nullopt;
        }

        stream << *encoded_value;
    }

    stream << END_SYMBOL;

    return stream.str();
}

}  // namespace internal

}  // namespace bencode
