#include "client.hpp"

#include <cstddef>
#include <cstdint>
#include <curl/curl.h>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <magic_enum.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/transform.hpp>

#include "bencode/decoders.hpp"
#include "bencode/types.hpp"
#include "misc/curl.hpp"
#include "misc/url.hpp"

namespace torrent::client {

constexpr const std::size_t PEERS_CHUNK_SIZE_BYTES = 6;

int hex_str_to_int(std::string hex_str);
auto sha1_hash_to_bytes(std::string_view hash) -> std::vector<uint8_t>;
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

    fmt::println("Fetching peers from url: {}", url.to_string());

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

auto sha1_hash_to_bytes(std::string_view hash) -> std::vector<uint8_t>
{
    using namespace ::ranges;

    auto hex_str_to_int = [](std::string hex_str) {
        return std::stoi(hex_str, 0, 16);
    };

    // clang-format off
    return hash
           | views::chunk(2)
           | views::transform([&](auto chunk) {
                return hex_str_to_int({chunk.begin(), chunk.end()});
             })
           | to<std::vector<uint8_t>>();
    // clang-format on
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
