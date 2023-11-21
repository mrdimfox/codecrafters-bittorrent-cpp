#include <cctype>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

json decode_bencoded_value(const std::string& encoded_value);
auto is_encoded_integer(const std::string& encoded_string) -> bool;
json decode_integer(const std::string& encoded_integer);
json decode_string(const std::string& encoded_string);

int main(int argc, char* argv[])
{
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
        json decoded_value = decode_bencoded_value(encoded_value);
        fmt::println("{}", decoded_value.dump());
    }
    else {
        fmt::print(stderr, "unknown command: ");
        return 1;
    }

    return 0;
}


json decode_bencoded_value(const std::string& encoded_value)
{
    if (std::isdigit(encoded_value[0])) {
        return decode_string(encoded_value);
    }

    if (is_encoded_integer(encoded_value)) {
        return decode_integer(encoded_value);
    }

    throw std::runtime_error("Unhandled encoded value: " + encoded_value);
}

/**
 * @brief Decode bencoded string to json object
 *
 * @param encoded_string_value
 * @return json
 */
json decode_string(const std::string& encoded_string)
{
    // strings: "5:hello" -> "hello"
    size_t colon_index = encoded_string.find(':');
    if (colon_index != std::string::npos) {
        std::string number_string = encoded_string.substr(0, colon_index);
        int64_t number = std::atoll(number_string.c_str());
        std::string str = encoded_string.substr(colon_index + 1, number);
        return json(str);
    }
    else {
        throw std::runtime_error("Invalid encoded value: " + encoded_string);
    }
}

auto is_encoded_integer(const std::string& encoded_string) -> bool
{
    if (not encoded_string.starts_with("i")) {
        return false;
    }

    auto pos = encoded_string.find_first_of("e", 1);
    if (pos == std::string::npos) {
        return false;
    }

    if (pos != encoded_string.length() - 1) {
        return false;
    }

    return true;
}

/**
 * @brief Decode bencoded integer to json object
 *
 * @param encoded_integer
 * @return json
 */
json decode_integer(const std::string& encoded_integer)
{
    // strings: "i-123e" -> -123
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
