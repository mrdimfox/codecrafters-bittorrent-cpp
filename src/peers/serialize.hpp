#pragma once

#include <cstdint>
#include <vector>

#include "peers/types.hpp"

namespace torrent::peers {

auto pack_interested_msg() -> std::vector<uint8_t>;
auto pack_unchoke_msg() -> std::vector<uint8_t>;
auto pack_request_msg(uint32_t piece_idx, uint32_t begin, uint32_t length)
  -> std::vector<uint8_t>;

auto pack_handshake(PeerHandshakeMsg msg) -> std::vector<uint8_t>;


namespace internal {

auto pack_msg_header(MsgId msg_id, size_t length) -> std::vector<uint8_t>;

}

}  // namespace torrent::peers
