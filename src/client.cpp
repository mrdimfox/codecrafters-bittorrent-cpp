#include "client.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <curl/curl.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <magic_enum.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "bencode/decoders.hpp"
#include "bencode/types.hpp"
#include "misc/curl.hpp"
#include "misc/sha1.hpp"
#include "misc/tcp_transfer.hpp"
#include "misc/url.hpp"
#include "peers/peers.hpp"
#include "peers/types.hpp"
#include "torrent.hpp"

namespace torrent::client {

constexpr const std::size_t PEERS_CHUNK_SIZE_BYTES = 6;

auto decode_peers(bencode::Json::binary_t& peers) -> std::vector<std::string>;
auto decode_ip_port_binary(std::span<std::uint8_t> bytes) -> std::string;

auto get_peers(const Metainfo& meta) -> std::vector<std::string>
{
    using namespace ::ranges;

    curl::InitContext context;

    curl::Url url;
    url.base(meta.announce)
      .query("peer_id", "00112233445566778899")
      .query("port", 6881)
      .query("uploaded", 0)
      .query("downloaded", 0)
      .query("left", meta.length)
      .query("compact", 1)
      .query("info_hash", sha1_hash_to_bytes(meta.hash()));

    spdlog::debug("Fetching peers from url: {}", url.to_string());

    auto [code, response] = curl::Curl().get(url.to_string());
    if (code != CURLcode::CURLE_OK) {
        throw std::runtime_error(fmt::format(
          "Error while fetching peers: code {} ({})",
          magic_enum::enum_name(code), int(code)
        ));
    }

    std::string response_str{response.begin(), response.end()};

    auto decoded_result = bencode::decode_bencoded_value(response_str);

    if (not decoded_result) {
        throw std::runtime_error(
          fmt::format("Bad response from server:\n{}", response_str)
        );
    }

    auto [_, result_json] = *decoded_result;

    if (result_json.contains("failure reason")) {
        throw std::runtime_error(fmt::format(
          "Error while fetching peers: {}", result_json["failure reason"].dump()
        ));
    }

    auto peers = result_json.value("peers", bencode::Json::binary_t());

    return decode_peers(peers);
}

auto peer_handshake(std::string ip, std::string port, const Metainfo& meta)
  -> std::string
{
    curl::InitContext context;

    auto msg = peers::pack_handshake(peers::PeerHandshakeMsg{
      .info_hash = meta.hash(), .peer_id = "00112233445566778899"
    });

    auto [code, data] = curl::Curl().tcp_transfer(ip, port, msg);

    if (code != CURLcode::CURLE_OK) {
        throw std::runtime_error(fmt::format(
          "Error while peer handshake: code {} ({})",
          magic_enum::enum_name(code), curl_easy_strerror(code)
        ));
    }

    auto answer = peers::unpack_handshake(data);

    return answer.peer_id;
}

auto download_piece(
  const Metainfo& meta,
  std::string peer_ip,
  std::string peer_port,
  std::size_t piece_idx
) -> std::vector<uint8_t>
{
    using namespace torrent;
    using namespace asio::ip;
    using namespace std::chrono_literals;

    asio::io_context io;
    tcp::endpoint endpoint(make_address(peer_ip), std::stoull(peer_port));
    tcp::socket socket(io);
    socket.connect(endpoint);

    {
        spdlog::debug("HANDSHAKE");

        if (not socket.is_open()) {
            throw std::runtime_error(fmt::format(
              "Can't open socket of address {}:{}", peer_ip, peer_port
            ));
        }

        auto handshake_msg = peers::pack_handshake(peers::PeerHandshakeMsg{
          .info_hash = meta.hash(), .peer_id = "00112233445566778899"
        });

        auto result =
          net::tcp::exchange(io, socket, handshake_msg, handshake_msg.size())
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        auto answer = peers::unpack_handshake(*result);
        spdlog::debug("Info hash: {}", answer.info_hash);

        if (answer.info_hash != meta.hash()) {
            throw std::runtime_error(fmt::format(
              "Invalid info hash. Expected {}, got {}", meta.hash(),
              answer.info_hash
            ));
        }
    }

    {
        spdlog::debug("BITFIELD");

        auto result =
          net::tcp::read(io, socket, peers::MsgHeader::SIZE_IN_BYTES)
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        auto answer = peers::unpack_msg_header(*result).map_error([](auto&& e) {
            throw std::runtime_error(fmt::format(
              "Unexpected answer from peer: {}", magic_enum::enum_name(e)
            ));
        });

        spdlog::debug("Read: {}", magic_enum::enum_name(answer->id));

        if (answer->id == peers::MsgId::Bitfield) {
            net::tcp::read(io, socket, answer->body_length)
              .map_error([](auto&& e) {
                  throw std::runtime_error(
                    fmt::format("Connection error: {}", e.message())
                  );
              });
        }
        else {
            throw std::runtime_error(fmt::format(
              "Unexpected msg id from peer: {}. Expected Bitfield ({})",
              magic_enum::enum_integer(answer->id),
              magic_enum::enum_integer(peers::MsgId::Bitfield)
            ));
        }
    }

    {
        spdlog::debug("INTERESTED");

        auto interested_msg = peers::pack_interested_msg();

        auto result =
          net::tcp::exchange(
            io, socket, interested_msg, peers::MsgHeader::SIZE_IN_BYTES
          )
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        auto response =
          peers::unpack_msg_header(*result).map_error([](auto&& e) {
              throw std::runtime_error(fmt::format(
                "Unexpected answer from peer: {}", magic_enum::enum_name(e)
              ));
          });

        spdlog::debug("Answer: {}", magic_enum::enum_name(response->id));

        if (response->id != peers::MsgId::Unchoke) {
            throw std::runtime_error(fmt::format(
              "Peer not ready to transmit data: peer answer {}",
              magic_enum::enum_name(response->id)
            ));
        }
    }

    std::vector<uint8_t> piece;

    const auto last_piece_len = meta.length % meta.piece_length;
    const auto piece_len = (piece_idx == meta.pieces().size() - 1)
                             ? last_piece_len
                             : meta.piece_length;

    const auto max_block_len = 16 * 1024;

    while (piece.size() != piece_len) {
        const auto remain_bytes = piece_len - piece.size();

        const auto block_len =
          remain_bytes > max_block_len ? max_block_len : remain_bytes;

        spdlog::debug("REQUEST {} bytes", block_len);

        auto request_msg =
          peers::pack_request_msg(piece_idx, piece.size(), block_len);

        auto request_answer =
          net::tcp::exchange(
            io, socket, request_msg, peers::MsgHeader::SIZE_IN_BYTES
          )
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        spdlog::debug("READ PIECE");

        auto piece_header =
          peers::unpack_msg_header(*request_answer).map_error([](auto&& e) {
              throw std::runtime_error(fmt::format(
                "Unexpected answer from peer: {}", magic_enum::enum_name(e)
              ));
          });

        if (piece_header->id != peers::MsgId::Piece) {
            throw std::runtime_error(fmt::format(
              "Peer not ready to transmit data: peer answer {}",
              magic_enum::enum_name(piece_header->id)
            ));
        }

        spdlog::debug(
          "Answer: {}, block len: {}", magic_enum::enum_name(piece_header->id),
          piece_header->body_length
        );

        auto piece_body =  //
          net::tcp::read(io, socket, piece_header->body_length)
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        auto piece_msg =
          peers::unpack_piece_msg(*piece_body).map_error([](auto&& e) {
              throw std::runtime_error(fmt::format(
                "Unexpected answer from peer: {}", magic_enum::enum_name(e)
              ));
          });

        spdlog::debug(
          "Piece: idx {}, begin: {}", piece_msg->index, piece_msg->begin
        );

        piece.insert(
          piece.end(), piece_msg->block.begin(), piece_msg->block.end()
        );
    }

    socket.shutdown(asio::ip::tcp::socket::shutdown_receive);
    io.stop();

    return piece;
}

auto decode_peers(bencode::Json::binary_t& peers) -> std::vector<std::string>
{
    using namespace ::ranges;

    if (peers.size() % PEERS_CHUNK_SIZE_BYTES != 0) {
        throw std::runtime_error(fmt::format(
          "Bad response from server. \"peers\" field must be derisible by {}. "
          "Got length: {}",
          PEERS_CHUNK_SIZE_BYTES, peers.size()
        ));
    }

    // clang-format off
    auto peers_ips =
      peers 
      | views::chunk(PEERS_CHUNK_SIZE_BYTES) 
      | views::transform([](auto chunk) {
          return decode_ip_port_binary({chunk.begin(), chunk.end()});
      }) 
      | to<std::vector<std::string>>();
    // clang-format on

    return peers_ips;
}

auto decode_ip_port_binary(std::span<std::uint8_t> bytes) -> std::string
{
    auto ip =
      fmt::format("{}.{}.{}.{}", bytes[0], bytes[1], bytes[2], bytes[3]);

    auto port = bytes[4] << 8 | bytes[5];

    return fmt::format("{0}:{1}", ip, port);
}

}  // namespace torrent::client
