#include <cctype>  // is_digit
#include <optional>
#include <string>
#include <string_view>

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

    auto value_type_ahead = detect_bencoded_value_type(encoded_value);

    switch (value_type_ahead) {
        case EncodedValueType::String:
            return decode_string(encoded_value);

        case EncodedValueType::Integer:
            return decode_integer(encoded_value);

        case EncodedValueType::List:
            return decode_bencoded_list(encoded_value);

        case EncodedValueType::Dictionary:
            return decode_bencoded_dict(encoded_value);

        default:
            return std::nullopt;
            break;
    }
}

namespace internal {

auto detect_bencoded_value_type(std::string_view bencoded_value)
  -> EncodedValueType
{
    if (is_encoded_integer_ahead(bencoded_value)) {
        return EncodedValueType::Integer;
    }
    if (is_encoded_string_ahead(bencoded_value)) {
        return EncodedValueType::String;
    }
    if (is_encoded_list_ahead(bencoded_value)) {
        return EncodedValueType::List;
    }
    if (is_encoded_dict_ahead(bencoded_value)) {
        return EncodedValueType::Dictionary;
    }
    return EncodedValueType::Unknown;
}

/**
 * @brief Detects if it is an string ahead
 */
auto is_encoded_string_ahead(std::string_view encoded_value) -> bool
{
    return std::isdigit(encoded_value[0]);
}

/**
 * @brief Decode bencoded string to a pair of encoded value and decoded json
 *  object
 *
 * If string cannot be fully decoded as UTF-8 sequence (ascii only) it will be
 * decoded as Json::binary.
 *
 * "5:hello" -> "hello"
 * "3:\1\2\3" -> b"\1\2\3"
 */
auto decode_string(std::string_view encoded_string)
  -> std::optional<DecodedValue>
{
    size_t delimiter_index = encoded_string.find(STRING_DELIMITER_SYMBOL);

    if (delimiter_index == std::string::npos) {
        return std::nullopt;
    }

    auto len_str = encoded_string.substr(0, delimiter_index);

    auto len = to_integer(len_str);
    if (not len) {
        return std::nullopt;
    }

    auto last_symbol_pos = delimiter_index + 1 + *len;
    if (last_symbol_pos > encoded_string.length()) {
        return std::nullopt;
    }

    auto decoded_str = encoded_string.substr(delimiter_index + 1, *len);

    Json decoded_json_string(decoded_str);

    // TODO: Remove this hack
    // Try to convert json to string. If it doesn't work, decoded value is
    // probably just an array of bytes.
    try {
        decoded_json_string.dump(-1, ' ', /*ensure_ascii=*/true);

        return {
          {EncodedValue{
             .type = EncodedValueType::String,
             .value = std::string_view(
               encoded_string.begin(),
               std::next(encoded_string.begin(), last_symbol_pos)
             )
           },
           Json(decoded_str)}
        };
    } catch (const Json::type_error&) {
        return {
          {EncodedValue{
             .type = EncodedValueType::String,
             .value = std::string_view(
               encoded_string.begin(),
               std::next(encoded_string.begin(), last_symbol_pos)
             )
           },
           Json::binary(
             std::vector<std::uint8_t>(decoded_str.begin(), decoded_str.end())
           )}
        };
    }
}


/**
 * @brief Detects if it is an integer ahead
 */
auto is_encoded_integer_ahead(std::string_view encoded_value) -> bool
{
    return encoded_value.starts_with(INTEGER_START_SYMBOL);
}

/**
 * @brief Decode bencoded integer to json object
 *
 * "i-123e" -> -123
 */
auto decode_integer(std::string_view encoded_value)
  -> std::optional<DecodedValue>
{
    auto end_index = encoded_value.find_first_of(END_SYMBOL);
    if (end_index == std::string::npos) {
        return std::nullopt;
    }

    std::string_view encoded_integer(encoded_value.begin(), end_index + 1);

    auto integer_str = encoded_integer;
    integer_str.remove_prefix(1);
    integer_str.remove_suffix(1);

    auto decoded_int = to_integer(integer_str);
    if (not decoded_int) {
        return std::nullopt;
    }

    return {
      {EncodedValue{
         .type = EncodedValueType::Integer,
         .value = std::string_view(encoded_integer.begin(), end_index + 1)
       },
       Json(*decoded_int)}
    };
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

    while (not remaining_encoded_list.starts_with(END_SYMBOL)) {
        auto decoded = decode_bencoded_value(remaining_encoded_list);

        if (decoded) {
            auto [encoded, result] = *decoded;
            list.push_back(result);

            remaining_encoded_list.remove_prefix(encoded.value.length());
        }
        else {
            return std::nullopt;  // malformed list
        }
    }

    remaining_encoded_list.remove_prefix(1);  // rm list end symbol

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

    while (not remaining_encoded_dict.starts_with(END_SYMBOL)) {
        // Decode key (always string)
        auto decoded_key = decode_string(remaining_encoded_dict);
        if (not decoded_key) {
            return std::nullopt;
        }
        auto [encoded_key, key] = *decoded_key;
        remaining_encoded_dict.remove_prefix(encoded_key.value.length());

        // Decode value
        auto decoded_value = decode_bencoded_value(remaining_encoded_dict);
        if (not decoded_value) {
            return std::nullopt;
        }

        auto [encoded_value, value] = *decoded_value;
        remaining_encoded_dict.remove_prefix(encoded_value.value.length());

        // Insert into the dict
        dict.insert({std::string(key), value});
    }

    remaining_encoded_dict.remove_prefix(1);  // remove dict end symbol

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
