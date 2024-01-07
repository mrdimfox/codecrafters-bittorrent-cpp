#include "piece.hpp"

#include <cstddef>
#include <spdlog/spdlog.h>

#include "misc/tcp_transfer.hpp"
#include "peers/deserialize.hpp"
#include "peers/serialize.hpp"

namespace torrent::client {

auto PieceWorker::_download_piece(size_t piece_idx) -> void
{
    using namespace ::torrent;

    _ostream.clear();
    _ostream.seekp(0);

    spdlog::debug(
      "Receive piece {} from peer {}:{}", piece_idx, peer_ip, peer_port
    );

    if (not is_peer_connection_established) {
        _connect_to_peer();
    }

    const auto last_piece_len = meta.length % meta.piece_length;
    const auto piece_len = (piece_idx == meta.pieces().size() - 1)
                             ? last_piece_len
                             : meta.piece_length;

    const auto max_block_len = 16 * 1024;
    size_t received_bytes = 0;

    spdlog::debug("{}:{} Read piece {}", peer_ip, peer_port, piece_idx);

    while (received_bytes != piece_len) {
        const auto remain_bytes = piece_len - received_bytes;

        const auto block_len =
          remain_bytes > max_block_len ? max_block_len : remain_bytes;

        spdlog::debug("{}:{} Request {} bytes", peer_ip, peer_port, block_len);

        auto request_msg =
          peers::pack_request_msg(piece_idx, received_bytes, block_len);

        auto request_answer =
          net::tcp::exchange(
            io, socket, request_msg, peers::MsgHeader::SIZE_IN_BYTES
          )
            .map_error([this](auto&& e) {
                throw std::runtime_error(fmt::format(
                  "{}:{} Connection error: {}", peer_ip, peer_port, e.message()
                ));
            });

        auto piece_header =
          peers::unpack_msg_header(*request_answer).map_error([this](auto&& e) {
              throw std::runtime_error(fmt::format(
                "{}:{} Unexpected answer from peer: {}", peer_ip, peer_port,
                magic_enum::enum_name(e)
              ));
          });

        if (piece_header->id != peers::MsgId::Piece) {
            throw std::runtime_error(fmt::format(
              "{}:{} Peer not ready to transmit data: peer answer {}", peer_ip,
              peer_port, magic_enum::enum_name(piece_header->id)
            ));
        }

        spdlog::debug(
          "{}:{} Piece answer: {}, block len: {}", peer_ip, peer_port,
          magic_enum::enum_name(piece_header->id), piece_header->body_length
        );

        auto piece_body =  //
          net::tcp::read(io, socket, piece_header->body_length)
            .map_error([this](auto&& e) {
                throw std::runtime_error(fmt::format(
                  "{}:{} Connection error: {}", peer_ip, peer_port, e.message()
                ));
            });

        auto piece_msg =
          peers::unpack_piece_msg(*piece_body).map_error([this](auto&& e) {
              throw std::runtime_error(fmt::format(
                "{}:{} Unexpected answer from peer: {}", peer_ip, peer_port,
                magic_enum::enum_name(e)
              ));
          });

        spdlog::debug(
          "{}:{} Piece received: idx {}, begin: {}", peer_ip, peer_port,
          piece_msg->index, piece_msg->begin
        );

        _ostream.write(
          reinterpret_cast<const char*>(piece_msg->block.data()),
          piece_msg->block.size()
        );

        received_bytes += piece_msg->block.size();
    }

    _ostream.flush();
}

void PieceWorker::_connect_to_peer()
{
    spdlog::debug("Handshake with peer {}:{}", peer_ip, peer_port);

    socket.connect(endpoint);
    _do_handshake();
    _do_bitfield();
    _do_interested();
    is_peer_connection_established = true;
}

void PieceWorker::_do_handshake()
{
    spdlog::debug("HANDSHAKE");

    if (not socket.is_open()) {
        throw std::runtime_error(
          fmt::format("Can't open socket of address {}:{}", peer_ip, peer_port)
        );
    }

    auto handshake_msg = peers::pack_handshake(peers::PeerHandshakeMsg{
      .info_hash = meta.hash(), .peer_id = "00112233445566778899"
    });

    auto result =
      net::tcp::exchange(io, socket, handshake_msg, handshake_msg.size())
        .map_error([](auto&& e) {
            throw std::runtime_error(
              fmt::format("Connection error: {}", e.message())
            );
        });

    auto answer = peers::unpack_handshake(*result);
    spdlog::debug("Info hash: {}", answer.info_hash);

    if (answer.info_hash != meta.hash()) {
        throw std::runtime_error(fmt::format(
          "Invalid info hash. Expected {}, got {}", meta.hash(),
          answer.info_hash
        ));
    }
}

auto PieceWorker::_do_bitfield() -> void
{
    spdlog::debug("BITFIELD");

    auto result = net::tcp::read(io, socket, peers::MsgHeader::SIZE_IN_BYTES)
                    .map_error([](auto&& e) {
                        throw std::runtime_error(
                          fmt::format("Connection error: {}", e.message())
                        );
                    });

    auto answer = peers::unpack_msg_header(*result).map_error([](auto&& e) {
        throw std::runtime_error(fmt::format(
          "Unexpected answer from peer: {}", magic_enum::enum_name(e)
        ));
    });

    spdlog::debug("Read: {}", magic_enum::enum_name(answer->id));

    if (answer->id == peers::MsgId::Bitfield) {
        net::tcp::read(io, socket, answer->body_length).map_error([](auto&& e) {
            throw std::runtime_error(
              fmt::format("Connection error: {}", e.message())
            );
        });
    }
    else {
        throw std::runtime_error(fmt::format(
          "Unexpected msg id from peer: {}. Expected Bitfield ({})",
          magic_enum::enum_integer(answer->id),
          magic_enum::enum_integer(peers::MsgId::Bitfield)
        ));
    }
}

auto PieceWorker::_do_interested() -> void
{
    spdlog::debug("INTERESTED");

    auto interested_msg = peers::pack_interested_msg();

    auto result = net::tcp::exchange(
                    io, socket, interested_msg, peers::MsgHeader::SIZE_IN_BYTES
    )
                    .map_error([](auto&& e) {
                        throw std::runtime_error(
                          fmt::format("Connection error: {}", e.message())
                        );
                    });

    auto response = peers::unpack_msg_header(*result).map_error([](auto&& e) {
        throw std::runtime_error(fmt::format(
          "Unexpected answer from peer: {}", magic_enum::enum_name(e)
        ));
    });

    spdlog::debug("Answer: {}", magic_enum::enum_name(response->id));

    if (response->id != peers::MsgId::Unchoke) {
        throw std::runtime_error(fmt::format(
          "Peer not ready to transmit data: peer answer {}",
          magic_enum::enum_name(response->id)
        ));
    }
}

}  // namespace torrent::client
