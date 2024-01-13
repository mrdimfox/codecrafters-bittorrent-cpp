#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <magic_enum.hpp>
#include <vector>

namespace torrent::proto {

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

struct MsgHeader
{
    static constexpr std::size_t SIZE_IN_BYTES = 5;

    MsgId id;
    uint32_t body_length;
};

enum class Error
{
    INCOMPLETE_MESSAGE,
    MALFORMED_MESSAGE,
    UNKNOW_MESSAGE_ID,
};

struct PeerHandshakeMsg
{
    constexpr static std::size_t HEADER_SIZE = 20;
    constexpr static std::size_t RESERVED_SIZE = 8;
    constexpr static std::size_t HASH_SIZE = 20;
    constexpr static std::size_t PEER_ID_SIZE = 20;

    const std::string_view header = "BitTorrent protocol";
    const std::array<uint8_t, RESERVED_SIZE> reserved{0};
    std::string info_hash;
    std::string peer_id;

    constexpr static size_t SIZE =
      HEADER_SIZE + RESERVED_SIZE + HASH_SIZE + PEER_ID_SIZE;
};

struct PieceMsg
{
    constexpr static std::size_t INDEX_SIZE = 4;
    constexpr static std::size_t BEGIN_SIZE = 4;
    constexpr static std::size_t MIN_BLOCK_SIZE = 0;

    constexpr static size_t MIN_SIZE = INDEX_SIZE + BEGIN_SIZE + MIN_BLOCK_SIZE;

    std::size_t index;
    std::size_t begin;
    std::vector<uint8_t> block;
};

struct HaveMsg
{
    constexpr static std::size_t INDEX_SIZE = 4;
    constexpr static size_t MIN_SIZE = INDEX_SIZE;
    std::size_t index;
};

}  // namespace torrent::proto
