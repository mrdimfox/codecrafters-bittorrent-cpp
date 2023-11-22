#include <cassert>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

enum class EncodedValueType
{
    Integer,
    String,
    List,
    Unknown,
};

struct EncodedValue
{
    EncodedValueType type;
    std::string_view value;
};

const char END_SYMBOL = 'e';
const char LIST_START_SYMBOL = 'l';
const char INTEGER_START_SYMBOL = 'i';
const char STRING_DELIMITER_SYMBOL = ':';

auto decode_bencoded_value(const std::string& encoded_value)
  -> std::optional<std::tuple<EncodedValue, json>>;

auto extract_integer(std::string_view encoded_value)
  -> std::optional<std::string_view>;
json decode_integer(const std::string&);

auto extract_string(std::string_view encoded_value)
  -> std::optional<std::string_view>;
json decode_string(const std::string&);

auto is_encoded_list(std::string_view) -> bool;
auto decode_bencoded_list(std::string_view)
  -> std::optional<std::tuple<EncodedValue, json>>;

auto extract_bencoded_value(const std::string&) -> EncodedValue;

auto to_integer(std::string_view) -> std::optional<long long int>;

void tests();


int main(int argc, char* argv[])
{
#ifdef ENABLE_TESTS
    tests();
#endif

    if (argc < 2) {
        fmt::println(stderr, "Usage {} decode <encoded_value>", argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "decode") {
        if (argc < 3) {
            fmt::println(stderr, "Usage {} decode <encoded_value>", argv[0]);
            return 1;
        }

        std::string encoded_value = argv[2];

        auto decoded = decode_bencoded_value(encoded_value);
        if (decoded) {
            auto [_, decoded_value] = *decoded;
            fmt::println("{}", decoded_value.dump());
        }
        else {
            fmt::print(stderr, "Error while decoding: {}", encoded_value);
        }
    }
    else {
        fmt::print(stderr, "unknown command: ");
        return 1;
    }

    return 0;
}


/**
 * @brief Try to decode source string and raise if it fails
 *
 * Returns pair of encoded value and result of decoding
 *
 * @param encoded_value
 * @return std::tuple<EncodedValue, json>
 */
auto decode_bencoded_value(const std::string& encoded_value)
  -> std::optional<std::tuple<EncodedValue, json>>
{
    auto bencoded_value = extract_bencoded_value(encoded_value);

    switch (bencoded_value.type) {
        case EncodedValueType::String:
            // TODO: Fix std::string
            return {
              {bencoded_value, decode_string(std::string{bencoded_value.value})}
            };

        case EncodedValueType::Integer:
            // TODO: Fix std::string
            return {
              {bencoded_value, decode_integer(std::string{bencoded_value.value})
              }
            };

        case EncodedValueType::List:
            return decode_bencoded_list(bencoded_value.value);

        default:
            return std::nullopt;
            break;
    }
}

/**
 * @brief Infer value type and return bounded encoded value string
 *
 * For lists right bound can not be determined.
 */
auto extract_bencoded_value(const std::string& encoded_value) -> EncodedValue
{
    if (auto value = extract_integer(encoded_value); value) {
        return EncodedValue{.type = EncodedValueType::Integer, .value = *value};
    }

    if (auto value = extract_string(encoded_value); value) {
        return EncodedValue{.type = EncodedValueType::String, .value = *value};
    }

    if (is_encoded_list(encoded_value)) {
        return EncodedValue{
          .type = EncodedValueType::List, .value = encoded_value
        };
    }

    return {EncodedValueType::Unknown};
}

/**
 * @brief Extract encoded string value or return nullopt
 */
auto extract_string(std::string_view encoded_value)
  -> std::optional<std::string_view>
{
    // String starts with a digit
    if (not std::isdigit(encoded_value[0])) {
        return std::nullopt;
    }

    // Find for delimiter between length and data
    auto colon_pos = encoded_value.find_first_of(STRING_DELIMITER_SYMBOL);
    if (colon_pos == std::string::npos) {
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
 *
 * @param encoded_string_value
 * @return json
 */
json decode_string(const std::string& encoded_string)
{
    size_t delimiter_index = encoded_string.find(STRING_DELIMITER_SYMBOL);
    if (delimiter_index != std::string::npos) {
        std::string number_string = encoded_string.substr(0, delimiter_index);
        int64_t number = std::atoll(number_string.c_str());
        std::string str = encoded_string.substr(delimiter_index + 1, number);
        return json(str);
    }
    else {
        throw std::runtime_error("Invalid encoded value: " + encoded_string);
    }
}

/**
 * @brief Extract encoded integer value or return nullopt
 */
auto extract_integer(std::string_view encoded_value)
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
 *
 * @return json
 */
json decode_integer(const std::string& encoded_integer)
{
    std::string_view encoded_integer_view{encoded_integer};
    encoded_integer_view.remove_prefix(1);
    encoded_integer_view.remove_suffix(1);

    auto to_integer = [&](std::string_view s) -> long long int {
        long long int value{};
        auto result = std::from_chars(s.data(), s.data() + s.size(), value);

        if (result.ec != std::errc{} or result.ptr != encoded_integer_view.end()) {
            throw std::runtime_error(
              "Decoding error. Suggested value type: integer. Found: " +
              encoded_integer
            );
        }

        return value;
    };

    return json(to_integer(encoded_integer_view));
}


/**
 * @brief Detect if list ahead
 */
auto is_encoded_list(std::string_view encoded_value) -> bool
{
    if (not encoded_value.starts_with(LIST_START_SYMBOL)) {
        return false;
    }

    return true;
}


/**
 * @brief Decode bencoded list of bencoded values to pair of source string and
 *        decoded json
 *
 * "l5:helloi52ee" -> ["hello", 52]
 */
auto decode_bencoded_list(std::string_view encoded_list)
  -> std::optional<std::tuple<EncodedValue, json>>
{
    std::string_view remaining_encoded_list{encoded_list};
    remaining_encoded_list.remove_prefix(1);  // rm "l" suffix
    std::size_t encoded_list_len = 1;         // 1 is "l" suffix length

    std::vector<json> list;

    // TODO: Fix while true
    while (true) {
        auto decoded =
          decode_bencoded_value(std::string(remaining_encoded_list));

        if (decoded) {
            auto [encoded, result] = *decoded;
            list.push_back(result);

            remaining_encoded_list.remove_prefix(encoded.value.length());
            encoded_list_len += encoded.value.length();
        }
        else {
            if (remaining_encoded_list[0] == END_SYMBOL) {
                encoded_list_len += 1;
                break;
            }
            else {
                return std::nullopt;
            }
        }
    }

    std::string_view encoded_original_list{
      encoded_list.begin(), encoded_list_len
    };

    return {
      {EncodedValue{
         .type = EncodedValueType::List,
         .value = encoded_original_list,
       },
       json(list)}
    };
}


auto to_integer(std::string_view s) -> std::optional<long long int>
{
    long long int value{};
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);

    if (result.ec != std::errc{} or result.ptr != s.end()) {
        return std::nullopt;
    }

    return value;
};


void tests()
{
    // extract_string
    {
        auto res = extract_string("3:test");
        assert(res != std::nullopt);
        assert(*res == std::string_view("3:tes"));
    }

    {
        auto res = extract_string("4:test");
        assert(res != std::nullopt);
        assert(*res == std::string_view("4:test"));
    }

    {
        auto res = extract_string("10:test");
        assert(res == std::nullopt);
    }

    {
        auto res = extract_string("10test");
        assert(res == std::nullopt);
    }

    {
        std::string_view encoded_string{"4:test1:a"};
        auto res = extract_string(encoded_string);
        assert(res != std::nullopt);
        assert(*res == std::string_view("4:test"));

        auto res2 =
          extract_string(std::string_view(res->end(), encoded_string.end()));
        assert(res2 != std::nullopt);
        assert(*res2 == std::string_view("1:a"));
    }

    // extract_integer
    {
        auto res = extract_integer("i123e");
        assert(res != std::nullopt);
        assert(*res == std::string_view("i123e"));
    }

    {
        auto res = extract_integer("i123ee");
        assert(res != std::nullopt);
        assert(*res == std::string_view("i123e"));
    }

    {
        auto res = extract_integer("i123");
        assert(res == std::nullopt);
    }

    // extract value
    {
        auto res = extract_bencoded_value("i123e");
        assert(res.type == EncodedValueType::Integer);
        assert(res.value == std::string_view("i123e"));
    }

    {
        auto res = extract_bencoded_value("3:test");
        assert(res.type == EncodedValueType::String);
        assert(res.value == std::string_view("3:tes"));
    }

    {
        auto res = extract_bencoded_value("le");
        assert(res.type == EncodedValueType::List);
        assert(res.value == std::string_view("le"));
    }

    {
        auto res = extract_bencoded_value("abcx");
        assert(res.type == EncodedValueType::Unknown);
    }

    // decode list
    {
        auto res = decode_bencoded_list("l2:abe");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == std::string_view("l2:abe"));

        auto expected_result = json(std::vector{json("ab")});
        assert(result.dump() == expected_result.dump());
    }

    {
        auto res = decode_bencoded_list("li123el2:abee");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == std::string_view("li123el2:abee"));

        auto expected_result =
          json(std::vector{json(123), json(std::vector{json("ab")})});

        assert(result.dump() == expected_result.dump());
    }

    {
        auto res = decode_bencoded_list("l2:aasdasdbe");
        assert(res == std::nullopt);
    }

    fmt::println("All tests passed");
}
