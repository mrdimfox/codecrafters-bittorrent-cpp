#include <cctype>  // is_digit
#include <optional>
#include <string>
#include <string_view>

#include <fmt/core.h>

#include "bencode/consts.hpp"
#include "bencode/decoders.hpp"
#include "bencode/tools.hpp"
#include "bencode/types.hpp"


using Json = nlohmann::json;


namespace bencode {

/**
 * @brief Try to decode source string and return nullopt if failed
 *
 * Returns pair of encoded value and result of decoding
 */
auto decode_bencoded_value(std::string_view encoded_value)
  -> std::optional<DecodedValue>
{
    using namespace internal;

    auto bencoded_value = extract_bencoded_value(encoded_value);

    switch (bencoded_value.type) {
        case EncodedValueType::String:
            return {{bencoded_value, decode_string(bencoded_value.value)}};

        case EncodedValueType::Integer:
            return {{bencoded_value, decode_integer(bencoded_value.value)}};

        case EncodedValueType::List:
            return decode_bencoded_list(bencoded_value.value);

        case EncodedValueType::Dictionary:
            return decode_bencoded_dict(bencoded_value.value);

        default:
            return std::nullopt;
            break;
    }
}

namespace internal {

/**
 * @brief Infer value type and return bounded encoded value string
 *
 * For lists and dicts right bound can not be determined.
 */
auto extract_bencoded_value(std::string_view encoded_value) -> EncodedValue
{
    if (auto value = extract_bencoded_integer(encoded_value); value) {
        return EncodedValue{.type = EncodedValueType::Integer, .value = *value};
    }

    if (auto value = extract_bencoded_string(encoded_value); value) {
        return EncodedValue{.type = EncodedValueType::String, .value = *value};
    }

    if (is_encoded_list_ahead(encoded_value)) {
        return EncodedValue{
          .type = EncodedValueType::List, .value = encoded_value
        };
    }

    if (is_encoded_dict_ahead(encoded_value)) {
        return EncodedValue{
          .type = EncodedValueType::Dictionary, .value = encoded_value
        };
    }

    return {EncodedValueType::Unknown};
}


/**
 * @brief Extract encoded string value or return nullopt
 */
auto extract_bencoded_string(std::string_view encoded_value)
  -> std::optional<std::string_view>
{
    // String starts with a digit
    if (not std::isdigit(encoded_value[0])) {
        return std::nullopt;
    }

    // Find for delimiter between length and data
    auto colon_pos = encoded_value.find_first_of(STRING_DELIMITER_SYMBOL);
    if (colon_pos == std::string_view::npos) {
        return std::nullopt;
    }

    std::string_view encoded_len{encoded_value.begin(), colon_pos};

    auto len = to_integer(encoded_len);
    if (not len) {
        return std::nullopt;
    }

    auto last_symbol_pos = colon_pos + 1 + *len;
    if (last_symbol_pos > encoded_value.length()) {
        return std::nullopt;
    }

    return std::string_view{encoded_value.begin(), last_symbol_pos};
}

/**
 * @brief Decode bencoded string to json object
 *
 * "5:hello" -> "hello"
 */
Json decode_string(std::string_view encoded_string)
{
    size_t delimiter_index = encoded_string.find(STRING_DELIMITER_SYMBOL);

    if (delimiter_index == std::string::npos) {
        throw std::runtime_error(
          fmt::format("Invalid encoded value: {}", encoded_string)
        );
    }

    auto number_string = encoded_string.substr(0, delimiter_index);

    auto number = to_integer(number_string);
    if (not number) {
        throw std::runtime_error(
          fmt::format("Invalid encoded value: {}", encoded_string)
        );
    }

    auto decoded_str = encoded_string.substr(delimiter_index + 1, *number);

    return Json(decoded_str);
}


/**
 * @brief Extract encoded integer value or return nullopt
 */
auto extract_bencoded_integer(std::string_view encoded_value)
  -> std::optional<std::string_view>
{
    if (not encoded_value.starts_with(INTEGER_START_SYMBOL)) {
        return std::nullopt;
    }

    auto pos = encoded_value.find_first_of(END_SYMBOL);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    return std::string_view{encoded_value.begin(), pos + 1};
}

/**
 * @brief Decode bencoded integer to json object
 *
 * "i-123e" -> -123
 */
Json decode_integer(std::string_view encoded_integer)
{
    auto original_encoded_int = encoded_integer;

    encoded_integer.remove_prefix(1);
    encoded_integer.remove_suffix(1);

    auto decoded_int = to_integer(encoded_integer);
    if (not decoded_int) {
        throw std::runtime_error(fmt::format(
          "Decoding error. Suggested value type: integer. Found: {}",
          original_encoded_int
        ));
    }

    return Json(*decoded_int);
}


/**
 * @brief Detect if list ahead
 */
auto is_encoded_list_ahead(std::string_view encoded_value) -> bool
{
    return encoded_value.starts_with(LIST_START_SYMBOL);
}


/**
 * @brief Decode bencoded list of bencoded values to pair of source string and
 *        decoded json
 *
 * "l5:helloi52ee" -> ["hello", 52]
 */
auto decode_bencoded_list(std::string_view encoded_list)
  -> std::optional<DecodedValue>
{
    std::string_view remaining_encoded_list{encoded_list};
    remaining_encoded_list.remove_prefix(1);  // rm "l" suffix

    std::vector<Json> list;

    // TODO: Fix while true
    while (true) {
        auto decoded = decode_bencoded_value(remaining_encoded_list);

        if (decoded) {
            auto [encoded, result] = *decoded;
            list.push_back(result);

            remaining_encoded_list.remove_prefix(encoded.value.length());
        }
        else {
            if (remaining_encoded_list.starts_with(END_SYMBOL)) {
                remaining_encoded_list.remove_prefix(1);
                break;  // entire list processed
            }
            else {
                return std::nullopt;  // malformed list
            }
        }
    }

    auto encoded_list_len =
      encoded_list.length() - remaining_encoded_list.length();

    std::string_view encoded_original_list{
      encoded_list.begin(), encoded_list_len
    };

    return {
      {EncodedValue{
         .type = EncodedValueType::List,
         .value = encoded_original_list,
       },
       Json(list)}
    };
}


/**
 * @brief Detect if dict ahead
 */
auto is_encoded_dict_ahead(std::string_view encoded_value) -> bool
{
    return encoded_value.starts_with(DICT_START_SYMBOL);
}


/**
 * @brief Decode bencoded dict of bencoded values to pair of source string and
 *        decoded json
 *
 * "d3:foo3:bar5:helloi52ee" -> {"hello": 52, "foo":"bar"}
 */
auto decode_bencoded_dict(std::string_view encoded_dict)
  -> std::optional<DecodedValue>
{
    std::string_view remaining_encoded_dict{encoded_dict};
    remaining_encoded_dict.remove_prefix(1);  // rm "d" suffix

    Dict dict;

    while (true) {
        // Decode key (always string)
        auto encoded_key = extract_bencoded_string(remaining_encoded_dict);
        if (not encoded_key) {
            return std::nullopt;
        }

        std::string key = decode_string(*encoded_key);
        remaining_encoded_dict.remove_prefix(encoded_key->length());

        // Decode value
        auto decoded_value = decode_bencoded_value(remaining_encoded_dict);
        if (not decoded_value) {
            return std::nullopt;
        }

        auto [encoded_value, value] = *decoded_value;
        remaining_encoded_dict.remove_prefix(encoded_value.value.length());

        // Insert into the dict
        dict.insert({key, value});

        // Check for dict end
        if (remaining_encoded_dict.starts_with(END_SYMBOL)) {
            remaining_encoded_dict.remove_prefix(1);
            break;  // entire dict processed
        }
    }

    auto encoded_dict_len =
      encoded_dict.length() - remaining_encoded_dict.length();

    std::string_view encoded_original_dict{
      encoded_dict.begin(), encoded_dict_len
    };

    return {
      {EncodedValue{
         .type = EncodedValueType::Dictionary,
         .value = encoded_original_dict,
       },
       Json(dict)}
    };
}


}  // namespace internal

}  // namespace bencode
