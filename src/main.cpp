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
    else if (command == "info" or command == "dump") {
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

        auto metainfo = torrent::Metainfo::from_file(torrent_file_path);
        if (not metainfo) {
            fmt::println(
              stderr, "Error while file decoding: {}", torrent_file_path.c_str()
            );
            return 1;
        }

        fmt::println(
          "Tracker URL: {}\nLength: {}", metainfo->announce, metainfo->length
        );

        if (command == "dump") {
            auto meta_dump =
              metainfo->raw.dump(4, 32, false, Json::error_handler_t::replace);

            if (meta_dump.length() > 1500) {
                fmt::println(
                  "[DEBUG] Torrent file meta:\n{}\n<...SKIPPED..>\n{}",
                  meta_dump.substr(0, 1000),
                  meta_dump.substr(meta_dump.length() - 500, meta_dump.length())
                );
            }
            else {
                fmt::println("[DEBUG] Torrent file meta:\n{}", meta_dump);
            }
        }
    }
    else {
        fmt::print(stderr, "Unknown command: ");
        return 1;
    }

    return 0;
}
