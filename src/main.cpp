#include <cstdio>  // for stderr
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "bencode/decoders.hpp"
#include "bencode/types.hpp"
#include "torrent.hpp"

using Json = nlohmann::json;

#ifdef ENABLE_TESTS
void tests();
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
#ifdef ENABLE_TESTS
    tests();
#endif

    if (argc < 2) {
        fmt::println(stderr, "Usage: {} decode <encoded_value>", argv[0]);
        fmt::println(stderr, "Usage: {} info <torrent_file_path>", argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "decode") {
        if (argc < 3) {
            fmt::println(stderr, "Usage: {} decode <encoded_value>", argv[0]);
            return 1;
        }

        std::string encoded_value = argv[2];

        auto decoded = bencode::decode_bencoded_value(encoded_value);
        if (decoded) {
            auto [_, decoded_value] = *decoded;
            fmt::println("{}", decoded_value.dump());
        }
        else {
            fmt::print(stderr, "Error while decoding: {}", encoded_value);
            return 1;
        }
    }
    else if (command == "info") {
        if (argc < 3) {
            fmt::println(stderr, "Usage: {} info <torrent_file_path>", argv[0]);
            return 1;
        }

        fs::path torrent_file_path(argv[2]);

        if (not fs::exists(torrent_file_path)) {
            fmt::print(
              stderr, "File not found: \"{}\"", torrent_file_path.c_str()
            );
            return 1;
        }

        auto metainfo = torrent::metainfo(torrent_file_path);
        if (not metainfo) {
            fmt::println(
              stderr, "Error while file decoding: {}", torrent_file_path.c_str()
            );
            return 1;
        }

        std::string announce = metainfo->value("announce", "unknown");
        auto length =
          metainfo->value("info", Json()).value<bencode::Integer>("length", 0);

        fmt::println(
          "Tracker URL: {}\nLength: {}",  //
          announce, length
        );
    }
    else {
        fmt::print(stderr, "Unknown command: ");
        return 1;
    }

    return 0;
}
