#include <cstdio>  // for stderr
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "bencode/consts.hpp"
#include "bencode/decoders.hpp"
#include "bencode/types.hpp"
#include "torrent.hpp"

using Json = nlohmann::json;
namespace fs = std::filesystem;

#ifdef ENABLE_TESTS
void tests();
#endif


#define EXPECTED(assertion, msg_c_str, args...)                                \
    do {                                                                       \
        if (not bool(assertion)) {                                             \
            fmt::println(stderr, "[ERROR] " msg_c_str, args);                  \
            return ExitCode::Fail;                                             \
        }                                                                      \
    } while (0)


enum ExitCode
{
    Success = EXIT_SUCCESS,
    Fail = EXIT_FAILURE,
};

auto decode_command(std::string encoded_value) -> ExitCode;
auto info_command(fs::path torrent_file_path) -> ExitCode;
auto dump_command(fs::path torrent_file_path) -> ExitCode;

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
        EXPECTED(argc == 3, "Usage: {} info <encoded_value>", argv[0]);
        return decode_command(argv[2]);
    }

    if (command == "info") {
        EXPECTED(argc == 3, "Usage: {} info <torrent_file_path>", argv[0]);
        return info_command(argv[2]);
    }

    if (command == "dump") {
        EXPECTED(argc == 3, "Usage: {} dump <torrent_file_path>", argv[0]);
        return dump_command(argv[2]);
    }

    fmt::print(stderr, R"(Unknown command: "{0}")", command);
    return ExitCode::Fail;
}


auto decode_command(std::string encoded_value) -> ExitCode
{
    auto decoded = bencode::decode_bencoded_value(encoded_value);
    EXPECTED(decoded.has_value(), "Error while decoding: {}", encoded_value);

    auto [_, decoded_value] = *decoded;
    fmt::println("{}", decoded_value.dump());

    return ExitCode::Success;
}

auto info_command(fs::path torrent_file_path) -> ExitCode
{
    EXPECTED(
      fs::exists(torrent_file_path), "File not found: \"{}\"",
      torrent_file_path.c_str()
    );

    auto metainfo = torrent::Metainfo::from_file(torrent_file_path);
    EXPECTED(
      metainfo.has_value(), "Error while file decoding: {}",
      torrent_file_path.c_str()
    );

    fmt::println(
      "Tracker URL: {0}\nLength: {1}\nInfo Hash: {2}\nPiece Length: {3}",
      metainfo->announce, metainfo->length, metainfo->hash(),
      metainfo->piece_length
    );

    const auto pieces = metainfo->pieces();

    EXPECTED(
      pieces.size() % 20 == 0,
      R"(pieces field length must be divisible by {}. Actual size {})",
      bencode::PIECE_HASH_LENGTH, pieces.size()
    );

    fmt::println("Piece Hashes:");

    for (auto iter = pieces.begin(); iter != pieces.end();
         iter += bencode::PIECE_HASH_LENGTH) {

        std::span hash_bytes{iter, std::next(iter, bencode::PIECE_HASH_LENGTH)};

        fmt::println("{:02x}", fmt::join(hash_bytes, ""));
    }

    return ExitCode::Success;
}

auto dump_command(fs::path torrent_file_path) -> ExitCode
{
    EXPECTED(
      fs::exists(torrent_file_path), "File not found: \"{}\"",
      torrent_file_path.c_str()
    );

    auto metainfo = torrent::Metainfo::from_file(torrent_file_path);
    EXPECTED(
      metainfo.has_value(), "Error while file decoding: {}",
      torrent_file_path.c_str()
    );

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

    return ExitCode::Success;
}
