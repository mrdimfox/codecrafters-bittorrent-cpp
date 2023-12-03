#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fmt/core.h"
#include "fmt/format.h"
#include "misc/sha1.hpp"

namespace torrent::peers {

struct PeerHandshakeMsg
{
    constexpr static size_t RESERVED_SIZE = 8;
    constexpr static size_t HASH_SIZE = 20;
    constexpr static size_t PEER_ID_SIZE = 20;

    const std::string_view header = "BitTorrent protocol";
    const std::array<uint8_t, RESERVED_SIZE> reserved{0};
    std::string info_hash;
    std::string peer_id;
};

template<typename Msg>
inline auto pack_msg(Msg) -> std::vector<uint8_t>;

template<typename Msg>
inline auto unpack_msg(std::span<uint8_t>) -> Msg;

template<>
inline auto pack_msg<PeerHandshakeMsg>(PeerHandshakeMsg msg)
  -> std::vector<uint8_t>
{
    std::vector<uint8_t> packed;

    packed.push_back(msg.header.size());
    packed.insert(packed.end(), msg.header.begin(), msg.header.end());
    packed.insert(packed.end(), msg.reserved.begin(), msg.reserved.end());

    const auto hash = sha1_hash_to_bytes(msg.info_hash);
    packed.insert(packed.end(), hash.begin(), hash.end());

    packed.insert(packed.end(), msg.peer_id.begin(), msg.peer_id.end());

    return packed;
}

template<>
inline auto unpack_msg<PeerHandshakeMsg>(std::span<uint8_t> msg)
  -> PeerHandshakeMsg
{
    auto header_len = static_cast<size_t>(msg[0]);
    std::span header{msg.begin() + 1, header_len};

    std::span hash{
      header.end() + PeerHandshakeMsg::RESERVED_SIZE,
      PeerHandshakeMsg::HASH_SIZE
    };

    std::span peer_id{hash.end(), PeerHandshakeMsg::PEER_ID_SIZE};

    return PeerHandshakeMsg{
      .info_hash = fmt::format("{:x}", fmt::join(hash, "")),
      .peer_id = fmt::format("{:02x}", fmt::join(peer_id, ""))
    };
}


}  // namespace torrent::peers
