#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>

#include "bencode/decoders.hpp"
#include "client/client.hpp"
#include "misc/parse_ip_port.hpp"
#include "spdlog/common.h"
#include "torrent.hpp"

using Json = nlohmann::json;
namespace fs = std::filesystem;

#ifdef ENABLE_TESTS
void tests();
#endif


#define EXPECTED(assertion, msg_c_str, args...)                                \
    do {                                                                       \
        if (not bool(assertion)) {                                             \
            spdlog::error(msg_c_str, args);                                    \
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
auto peers_command(fs::path torrent_file_path) -> ExitCode;
auto peer_handshake_command(
  fs::path torrent_file_path, std::string peer_ip_port
) -> ExitCode;
auto download_piece_command(
  fs::path torrent_file_path, fs::path output_file_path, std::size_t piece_idx
) -> ExitCode;
auto download_file_command(
  fs::path torrent_file_path, fs::path output_file_path
) -> ExitCode;


#include <range/v3/view.hpp>

int main(int argc, char* argv[])
{
    auto internal_logger = spdlog::stdout_color_mt("internal_logger");

#ifdef NDEBUG
    spdlog::set_level(spdlog::level::err);
    internal_logger->set_level(spdlog::level::off);
#else
    spdlog::set_level(spdlog::level::err);
    internal_logger->set_level(spdlog::level::off);
#endif

    if (argc < 2) {
        // clang-format off
        spdlog::error("Usage:");
        spdlog::error("  {} decode <encoded_value>", argv[0]);
        spdlog::error("  {} info <torrent_file_path>", argv[0]);
        spdlog::error("  {} dump <torrent_file_path>", argv[0]);
        spdlog::error("  {} peers <torrent_file_path>", argv[0]);
        spdlog::error("  {} handshake <torrent_file_path> <peer_ip>:<peer_port>", argv[0]);
        spdlog::error("  {} download_piece -o <output_file_path> <torrent_file_path> <piece_idx>", argv[0]);
        spdlog::error("  {} download -o <output_file_path> <torrent_file_path>", argv[0]);
        // clang-format on
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "test") {
#ifdef ENABLE_TESTS
            tests();
#endif
            return ExitCode::Success;
        }

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

        if (command == "peers") {
            EXPECTED(argc == 3, "Usage: {} peers <torrent_file_path>", argv[0]);
            return peers_command(argv[2]);
        }

        if (command == "handshake") {
            EXPECTED(
              argc == 4,
              "Usage: {} handshake <torrent_file_path> <peer_ip>:<peer_port>",
              argv[0]
            );

            return peer_handshake_command(argv[2], argv[3]);
        }

        if (command == "download_piece") {
            EXPECTED(
              argc == 6,
              "Usage: {} download_piece -o <output_file_path> "
              "<torrent_file_path> <piece_idx>",
              argv[0]
            );

            auto idx = std::stoull(argv[5]);

            return download_piece_command(argv[4], argv[3], idx);
        }

        if (command == "download") {
            EXPECTED(
              argc == 5,
              "Usage: {} download -o <output_file_path> "
              "<torrent_file_path>",
              argv[0]
            );

            return download_file_command(argv[4], argv[3]);
        }
    } catch (std::runtime_error e) {
        spdlog::error("{}", e.what());
        return ExitCode::Fail;
    }

    spdlog::error(R"(Unknown command: "{0}")", command);
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

    fmt::println("Piece Hashes:");

    for (auto&& piece : pieces) {
        fmt::println("{:02x}", fmt::join(piece, ""));
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
        spdlog::debug(
          "Torrent file meta:\n{}\n<...SKIPPED..>\n{}",
          meta_dump.substr(0, 1000),
          meta_dump.substr(meta_dump.length() - 500, meta_dump.length())
        );
    }
    else {
        spdlog::debug("Torrent file meta:\n{}", meta_dump);
    }

    return ExitCode::Success;
}

auto peers_command(fs::path torrent_file_path) -> ExitCode
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

    auto peers = torrent::client::get_peers(*metainfo);
    fmt::println("{}", fmt::join(peers, "\n"));

    return ExitCode::Success;
}


auto peer_handshake_command(
  fs::path torrent_file_path, std::string peer_ip_port
) -> ExitCode
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

    auto [peer_ip, peer_port] = utils::parse_ip_port(peer_ip_port);

    auto peer_id =
      torrent::client::peer_handshake(peer_ip, peer_port, *metainfo);

    fmt::println("Peer ID: {}", peer_id);

    return ExitCode::Success;
}


auto download_piece_command(
  fs::path torrent_file_path, fs::path output_file_path, std::size_t piece_idx
) -> ExitCode
{
    EXPECTED(
      fs::exists(torrent_file_path), "File not found: \"{}\"",
      torrent_file_path.c_str()
    );

    if (output_file_path.has_parent_path()) {
        EXPECTED(
          fs::exists(output_file_path.parent_path()), "Path not found: \"{}\"",
          torrent_file_path.parent_path().c_str()
        );
    }

    auto metainfo = torrent::Metainfo::from_file(torrent_file_path);
    EXPECTED(
      metainfo.has_value(), "Error while torrent file decoding: {}",
      torrent_file_path.c_str()
    );

    auto peers = torrent::client::get_peers(*metainfo);
    EXPECTED(
      peers.size() > 0, "No peers returned from server: {}", metainfo->announce
    );

    auto [peer_ip, peer_port] = utils::parse_ip_port(peers[0]);

    std::ofstream ofs;
    ofs.open(output_file_path, std::ios::out | std::ios::binary);
    EXPECTED(ofs, "Can't write to file {}", output_file_path.c_str());

    torrent::client::download_piece(
      *metainfo, peer_ip, peer_port, piece_idx, ofs
    );

    fmt::print(
      "Piece {} downloaded to {}.", piece_idx, output_file_path.c_str()
    );

    return ExitCode::Success;
}


auto download_file_command(
  fs::path torrent_file_path, fs::path output_file_path
) -> ExitCode
{
    EXPECTED(
      fs::exists(torrent_file_path), "File not found: \"{}\"",
      torrent_file_path.c_str()
    );

    if (output_file_path.has_parent_path()) {
        EXPECTED(
          fs::exists(output_file_path.parent_path()), "Path not found: \"{}\"",
          torrent_file_path.parent_path().c_str()
        );
    }

    auto metainfo = torrent::Metainfo::from_file(torrent_file_path);
    EXPECTED(
      metainfo.has_value(), "Error while torrent file decoding: {}",
      torrent_file_path.c_str()
    );

    auto peers = torrent::client::get_peers(*metainfo);
    EXPECTED(
      peers.size() > 0, "No peers returned from server: {}", metainfo->announce
    );

    std::ofstream ofs;
    ofs.open(output_file_path, std::ios::out | std::ios::binary);
    EXPECTED(ofs, "Can't write to file {}", output_file_path.c_str());

    torrent::client::download_file(*metainfo, peers, ofs);

    fmt::print(
      "Downloaded {} to {}.", torrent_file_path.c_str(),
      output_file_path.c_str()
    );

    return ExitCode::Success;
}
