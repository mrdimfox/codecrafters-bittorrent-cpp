#include "proto/serialize.hpp"

#include <cstdint>
#include <vector>

#include "misc/sha1.hpp"
#include "proto/types.hpp"
#include "proto/utils.hpp"

namespace torrent::proto {

using namespace internal;

auto pack_interested_msg() -> std::vector<uint8_t>
{
    return pack_msg_header(MsgId::Interested, 1);
}

auto pack_not_interested_msg() -> std::vector<uint8_t>
{
    return pack_msg_header(MsgId::NotInterested, 1);
}


auto pack_unchoke_msg() -> std::vector<uint8_t>
{
    return pack_msg_header(MsgId::Unchoke, 1);
}

auto pack_request_msg(uint32_t piece_idx, uint32_t begin, uint32_t length)
  -> std::vector<uint8_t>
{
    const auto piece_idx_bytes = utils::pack_u32(piece_idx);
    const auto begin_bytes = utils::pack_u32(begin);
    const auto length_bytes = utils::pack_u32(length);

    std::vector<uint8_t> msg;
    msg.insert(msg.end(), piece_idx_bytes.begin(), piece_idx_bytes.end());
    msg.insert(msg.end(), begin_bytes.begin(), begin_bytes.end());
    msg.insert(msg.end(), length_bytes.begin(), length_bytes.end());

    const auto header = pack_msg_header(MsgId::Request, msg.size() + 1);
    msg.insert(msg.begin(), header.begin(), header.end());

    return msg;
}

auto pack_have_msg(uint32_t piece_idx) -> std::vector<uint8_t>
{
    const auto piece_idx_bytes = utils::pack_u32(piece_idx);

    std::vector<uint8_t> msg;
    msg.insert(msg.end(), piece_idx_bytes.begin(), piece_idx_bytes.end());

    const auto header = pack_msg_header(MsgId::Have, msg.size() + 1);
    msg.insert(msg.begin(), header.begin(), header.end());

    return msg;
}

auto pack_handshake(PeerHandshakeMsg msg) -> std::vector<uint8_t>
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

}  // namespace torrent::proto


namespace torrent::proto::internal {

auto pack_msg_header(MsgId msg_id, size_t length) -> std::vector<uint8_t>
{
    auto packed = utils::pack_u32(uint32_t(length));
    packed.push_back(uint8_t(msg_id));
    return packed;
}

}  // namespace torrent::proto::internal
