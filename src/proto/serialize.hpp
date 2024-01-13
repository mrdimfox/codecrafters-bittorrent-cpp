#pragma once

#include <cstdint>
#include <vector>

#include "proto/types.hpp"

namespace torrent::proto {

auto pack_interested_msg() -> std::vector<uint8_t>;
auto pack_not_interested_msg() -> std::vector<uint8_t>;
auto pack_unchoke_msg() -> std::vector<uint8_t>;
auto pack_request_msg(uint32_t piece_idx, uint32_t begin, uint32_t length)
  -> std::vector<uint8_t>;
auto pack_have_msg(uint32_t piece_idx) -> std::vector<uint8_t>;

auto pack_handshake(PeerHandshakeMsg msg) -> std::vector<uint8_t>;


namespace internal {

auto pack_msg_header(MsgId msg_id, size_t length) -> std::vector<uint8_t>;

}

}  // namespace torrent::proto
