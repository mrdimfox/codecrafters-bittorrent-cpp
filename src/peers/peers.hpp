#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>
#include <magic_enum.hpp>
#include <tl/expected.hpp>

#include "misc/sha1.hpp"
#include "peers/utils.hpp"

namespace torrent::peers {

struct PeerHandshakeMsg
{
    constexpr static size_t HEADER_SIZE = 20;
    constexpr static size_t RESERVED_SIZE = 8;
    constexpr static size_t HASH_SIZE = 20;
    constexpr static size_t PEER_ID_SIZE = 20;

    const std::string_view header = "BitTorrent protocol";
    const std::array<uint8_t, RESERVED_SIZE> reserved{0};
    std::string info_hash;
    std::string peer_id;

    constexpr static size_t SIZE =
      HEADER_SIZE + RESERVED_SIZE + HASH_SIZE + PEER_ID_SIZE;
};

enum class MsgId : uint8_t
{
    Choke = 0,
    Unchoke,
    Interested,
    NotInterested,
    Have,
    Bitfield,
    Request,
    Piece,
    Cancel
};

enum class Error
{
    INCOMPLETE_MESSAGE,
    MALFORMED_MESSAGE,
    UNKNOW_MESSAGE_ID,
};

template<MsgId ID>
struct PeerMsg;

constexpr static size_t MSG_HEADER_SIZE = 5;  // length (4) + id (1)

struct MsgHeader
{
    uint32_t length;
    MsgId id;
};

template<>
struct PeerMsg<MsgId::Interested>
{
    static constexpr uint32_t LENGTH = 1;
};

template<>
struct PeerMsg<MsgId::Unchoke>
{
    static constexpr uint32_t LENGTH = 1;
};


template<typename Msg>
inline auto pack_msg(Msg) -> std::vector<uint8_t>;

template<typename Msg>
inline auto unpack_msg(std::span<uint8_t>) -> Msg;


inline auto pack_msg_header(MsgId msg_id, size_t length = 1) -> std::vector<uint8_t>
{
    auto packed = utils::pack_u32(uint32_t(length));
    packed.push_back(uint8_t(msg_id));
    return packed;
}


inline auto unpack_msg_header(std::span<uint8_t> msg)
  -> tl::expected<MsgHeader, Error>
{
    if (msg.size() < MSG_HEADER_SIZE) {
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

    return MsgHeader{length, *msg_id};
}

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


}  // namespace torrent::peers
