#pragma once

#include <cstdint>
#include <span>

#include <tl/expected.hpp>

#include "proto/types.hpp"

namespace torrent::proto {

auto unpack_msg_header(std::span<const uint8_t> msg)
  -> tl::expected<MsgHeader, Error>;

auto unpack_piece_msg(std::span<const uint8_t> msg)
  -> tl::expected<PieceMsg, Error>;

auto unpack_have_msg(std::span<const uint8_t> msg)
  -> tl::expected<HaveMsg, Error>;

auto unpack_handshake(std::span<const uint8_t> msg) -> PeerHandshakeMsg;

}  // namespace torrent::proto
