#pragma once

#include <cstdint>
#include <span>

#include <tl/expected.hpp>

#include "peers/types.hpp"

namespace torrent::peers {

auto unpack_msg_header(std::span<uint8_t> msg)
  -> tl::expected<MsgHeader, Error>;

auto unpack_piece_msg(std::span<uint8_t> msg)
  -> tl::expected<PieceMsg, Error>;

auto unpack_handshake(std::span<uint8_t> msg) -> PeerHandshakeMsg;

}  // namespace torrent::peers
