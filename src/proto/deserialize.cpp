#include "proto/deserialize.hpp"

#include <cstdint>
#include <iterator>
#include <span>

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <stdexcept>
#include <tl/expected.hpp>
#include <vector>

#include "proto/types.hpp"
#include "proto/utils.hpp"


namespace torrent::proto {

auto unpack_msg_header(std::span<const uint8_t> msg) -> tl::expected<MsgHeader, Error>
{
    if (msg.size() < MsgHeader::SIZE_IN_BYTES) {
        return tl::make_unexpected(Error::INCOMPLETE_MESSAGE);
    }

    const auto length =
      utils::unpack_u32({msg.begin(), std::next(msg.begin(), 3)});

    if (length < 1) {
        return tl::make_unexpected(Error::MALFORMED_MESSAGE);
    }

    auto msg_id = magic_enum::enum_cast<MsgId>(msg[4]);
    if (not msg_id) {
        return tl::make_unexpected(Error::UNKNOW_MESSAGE_ID);
    }

    return MsgHeader{*msg_id, length - 1};
}

auto unpack_piece_msg(std::span<const uint8_t> msg) -> tl::expected<PieceMsg, Error>
{
    if (msg.size() < PieceMsg::MIN_SIZE) {
        return tl::make_unexpected(Error::INCOMPLETE_MESSAGE);
    }

    auto iter = msg.begin();

    const auto index =
      utils::unpack_u32({iter, std::next(iter, PieceMsg::INDEX_SIZE)});

    std::advance(iter, PieceMsg::INDEX_SIZE);

    const auto begin =
      utils::unpack_u32({iter, std::next(iter, PieceMsg::BEGIN_SIZE)});

    std::advance(iter, PieceMsg::INDEX_SIZE);

    return PieceMsg{index, begin, std::vector(iter, msg.end())};
}

auto unpack_have_msg(std::span<const uint8_t> msg) -> tl::expected<HaveMsg, Error>
{
    if (msg.size() < HaveMsg::MIN_SIZE) {
        return tl::make_unexpected(Error::INCOMPLETE_MESSAGE);
    }

    const auto iter = msg.begin();
    const auto index =
      utils::unpack_u32({iter, std::next(iter, PieceMsg::INDEX_SIZE)});

    return HaveMsg{.index = index};
}

auto unpack_handshake(std::span<const uint8_t> msg) -> PeerHandshakeMsg
{
    if (msg.size() < PeerHandshakeMsg::SIZE) {
        throw std::runtime_error(
          "Bad PeerHandshakeMsg size. Message is incomplete!"
        );
    }

    auto header_len = static_cast<size_t>(msg[0]);
    std::span header{msg.begin() + 1, header_len};

    std::span hash{
      header.end() + PeerHandshakeMsg::RESERVED_SIZE,
      PeerHandshakeMsg::HASH_SIZE
    };

    std::span peer_id{hash.end(), PeerHandshakeMsg::PEER_ID_SIZE};

    return PeerHandshakeMsg{
      .info_hash = fmt::format("{:02x}", fmt::join(hash, "")),
      .peer_id = fmt::format("{:02x}", fmt::join(peer_id, ""))
    };
}

}  // namespace torrent::proto
