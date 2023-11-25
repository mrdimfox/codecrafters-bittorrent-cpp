#include <cstdio>  // for stderr
#include <string>

#include <fmt/core.h>

#include "decoding/decoders.hpp"

using namespace nlohmann::json_literals;

#ifdef ENABLE_TESTS
void tests();
#endif

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

        auto decoded = decoding::decode_bencoded_value(encoded_value);
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
