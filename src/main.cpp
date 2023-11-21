#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

json decode_bencoded_value(const std::string& encoded_value)
{
    if (std::isdigit(encoded_value[0])) {
        // Example: "5:hello" -> "hello"
        size_t colon_index = encoded_value.find(':');
        if (colon_index != std::string::npos) {
            std::string number_string = encoded_value.substr(0, colon_index);
            int64_t number = std::atoll(number_string.c_str());
            std::string str = encoded_value.substr(colon_index + 1, number);
            return json(str);
        }
        else {
            throw std::runtime_error("Invalid encoded value: " + encoded_value);
        }
    }
    else {
        throw std::runtime_error("Unhandled encoded value: " + encoded_value);
    }
}

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

        // Uncomment this block to pass the first stage
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
