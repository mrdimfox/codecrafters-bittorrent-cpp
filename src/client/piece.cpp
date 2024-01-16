#include "piece.hpp"

#include <cstddef>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>

#include "fmt/core.h"
#include "misc/sha1.hpp"
#include "misc/tcp_transfer.hpp"
#include "proto/deserialize.hpp"
#include "proto/serialize.hpp"
#include "proto/types.hpp"

namespace torrent::client {

constexpr const auto MAX_BLOCK_LEN = 16 * 1024;

auto PieceWorker::_download_piece(size_t piece_idx) -> void
{
    using namespace ::torrent;

    _buffer.consume(_buffer.size());  // clear buffer

    if (_have_piece_idx) {
        piece_idx = _have_piece_idx.value();
        _have_piece_idx = std::nullopt;
    }
    else if (this->have_mode()) {
        if (not _wait_have()) {
            return;
        }
        piece_idx = _have_piece_idx.value();
        _have_piece_idx = std::nullopt;
    }

    spdlog::debug(
      "{}:{} Start receiving piece {} from peer", peer_ip, peer_port, piece_idx
    );

    if (not is_peer_connection_established) {
        _connect_to_peer();
    }

    const auto piece_hashes = meta.pieces();
    const auto last_piece_len = meta.length % meta.piece_length;
    const auto piece_len = (piece_idx == piece_hashes.size() - 1)
                             ? last_piece_len
                             : meta.piece_length;

    size_t received_bytes = 0;

    spdlog::debug("{}:{} Read piece {}", peer_ip, peer_port, piece_idx);

    while (received_bytes != piece_len) {
        const auto remain_bytes = piece_len - received_bytes;

        const auto block_len =
          remain_bytes > MAX_BLOCK_LEN ? MAX_BLOCK_LEN : remain_bytes;

        spdlog::debug("{}:{} Request {} bytes", peer_ip, peer_port, block_len);

        auto request_msg =
          proto::pack_request_msg(piece_idx, received_bytes, block_len);

        auto request_answer =
          net::tcp::exchange(
            io, socket, request_msg, proto::MsgHeader::SIZE_IN_BYTES
          )
            .map_error([this](auto&& e) {
                throw std::runtime_error(fmt::format(
                  "{}:{} Connection error: {}", peer_ip, peer_port, e.message()
                ));
            });

        auto piece_header =
          proto::unpack_msg_header(*request_answer).map_error([this](auto&& e) {
              throw std::runtime_error(fmt::format(
                "{}:{} Unexpected answer from peer: {}", peer_ip, peer_port,
                magic_enum::enum_name(e)
              ));
          });

        if (piece_header->id != proto::MsgId::Piece) {
            throw std::runtime_error(fmt::format(
              "{}:{} Peer not ready to transmit data: peer answer {}", peer_ip,
              peer_port, magic_enum::enum_name(piece_header->id)
            ));
        }

        spdlog::debug(
          "{}:{} Piece answer: {}, block len: {}", peer_ip, peer_port,
          magic_enum::enum_name(piece_header->id),
          piece_header->body_length - proto::PieceMsg::BEGIN_SIZE -
            proto::PieceMsg::INDEX_SIZE
        );

        auto piece_body =  //
          net::tcp::read(io, socket, piece_header->body_length)
            .map_error([this](auto&& e) {
                throw std::runtime_error(fmt::format(
                  "{}:{} Connection error: {}", peer_ip, peer_port, e.message()
                ));
            });

        auto piece_msg =
          proto::unpack_piece_msg(*piece_body).map_error([this](auto&& e) {
              throw std::runtime_error(fmt::format(
                "{}:{} Unexpected answer from peer: {}", peer_ip, peer_port,
                magic_enum::enum_name(e)
              ));
          });

        spdlog::debug(
          "{}:{} Piece received: idx {}, begin: {}, block len: {}", peer_ip,
          peer_port, piece_msg->index, piece_msg->begin, piece_msg->block.size()
        );

        _ostream.write(
          reinterpret_cast<const char*>(piece_msg->block.data()),
          piece_msg->block.size()
        );

        received_bytes += piece_msg->block.size();

        _progress_callback(piece_idx, received_bytes, piece_len);
    }

    _progress_callback(piece_idx, received_bytes, piece_len);
    spdlog::debug("Count of bytes received: {0}", received_bytes);

    if (received_bytes != piece_len) {
        throw std::runtime_error("Piece integrity is broken");
    }

    _ostream.flush();

    _last_piece_idx = piece_idx;
    _have_piece_idx = std::nullopt;
}

void PieceWorker::_connect_to_peer()
{
    spdlog::debug("Handshake with peer {}:{}", peer_ip, peer_port);
    socket.connect(endpoint);

    _do_handshake();
    _do_bitfield_or_unchoke();
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

    auto handshake_msg = proto::pack_handshake(proto::PeerHandshakeMsg{
      .info_hash = meta.hash(), .peer_id = "00112233445566778899"
    });

    auto result =
      net::tcp::exchange(io, socket, handshake_msg, handshake_msg.size())
        .map_error([](auto&& e) {
            throw std::runtime_error(
              fmt::format("Connection error: {}", e.message())
            );
        });

    auto answer = proto::unpack_handshake(*result);
    spdlog::debug("Info hash: {}", answer.info_hash);

    if (answer.info_hash != meta.hash()) {
        throw std::runtime_error(fmt::format(
          "Invalid info hash. Expected {}, got {}", meta.hash(),
          answer.info_hash
        ));
    }
}

auto PieceWorker::_do_bitfield_or_unchoke() -> void
{
    spdlog::debug("BITFIELD");

    auto result = net::tcp::read(io, socket, proto::MsgHeader::SIZE_IN_BYTES)
                    .map_error([](auto&& e) {
                        throw std::runtime_error(
                          fmt::format("Connection error: {}", e.message())
                        );
                    });

    auto answer = proto::unpack_msg_header(*result).map_error([](auto&& e) {
        throw std::runtime_error(fmt::format(
          "Unexpected answer from peer: {}", magic_enum::enum_name(e)
        ));
    });

    spdlog::debug("Read: {}", magic_enum::enum_name(answer->id));

    if (answer->id == proto::MsgId::Bitfield) {
        net::tcp::read(io, socket, answer->body_length).map_error([](auto&& e) {
            throw std::runtime_error(
              fmt::format("Connection error: {}", e.message())
            );
        });
    }
    else if (answer->id == proto::MsgId::Unchoke) {
        spdlog::debug("Peer is ready.");
    }
    else {
        const auto msg = fmt::format(
          "Unexpected msg id from peer: {}. Expected Bitfield ({})",
          magic_enum::enum_integer(answer->id),
          magic_enum::enum_integer(proto::MsgId::Bitfield)
        );

        spdlog::debug(msg);

        throw std::runtime_error(msg);
    }
}

auto PieceWorker::_do_interested() -> void
{
    spdlog::debug("INTERESTED");

    auto interested_msg = proto::pack_interested_msg();

    auto result = net::tcp::exchange(
                    io, socket, interested_msg, proto::MsgHeader::SIZE_IN_BYTES
    )
                    .map_error([](auto&& e) {
                        throw std::runtime_error(
                          fmt::format("Connection error: {}", e.message())
                        );
                    });

    auto response = proto::unpack_msg_header(*result).map_error([](auto&& e) {
        throw std::runtime_error(fmt::format(
          "Unexpected answer from peer: {}", magic_enum::enum_name(e)
        ));
    });

    spdlog::debug("Answer: {}", magic_enum::enum_name(response->id));

    if (response->id == proto::MsgId::Have) {
        const auto body =
          net::tcp::read(io, socket, response->body_length)
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        const auto msg = proto::unpack_have_msg(*body).map_error([](auto&& e) {
            throw std::runtime_error(fmt::format(
              "Unexpected answer from peer: {}", magic_enum::enum_name(e)
            ));
        });

        _have_piece_idx = msg->index;
        _have_mode = true;
    }
    else if (response->id != proto::MsgId::Unchoke) {
        throw std::runtime_error(fmt::format(
          "Peer not ready to transmit data: peer answer {}",
          magic_enum::enum_name(response->id)
        ));
        _have_mode = false;
    }
}

void PieceWorker::_try_have()
{
    using namespace std::chrono_literals;

    spdlog::debug("TRY HAVE {} ->", _last_piece_idx);

    auto have_msg = proto::pack_have_msg(_last_piece_idx);

    auto result =  //
      net::tcp::exchange(
        io, socket, have_msg, proto::MsgHeader::SIZE_IN_BYTES, 3s
      );

    if (not result and result.error() == asio::error::operation_aborted) {
        spdlog::debug("NO HAVE");
        return;
    }
    else if (not result) {
        throw std::runtime_error(
          fmt::format("Connection error: {}", result.error().message())
        );
    }

    auto response = proto::unpack_msg_header(*result).map_error([](auto&& e) {
        throw std::runtime_error(fmt::format(
          "Unexpected answer from peer: {}", magic_enum::enum_name(e)
        ));
    });

    spdlog::debug("Answer: {}", magic_enum::enum_name(response->id));

    if (response->id == proto::MsgId::Have) {
        const auto body =
          net::tcp::read(io, socket, response->body_length)
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        const auto msg = proto::unpack_have_msg(*body).map_error([](auto&& e) {
            throw std::runtime_error(fmt::format(
              "Unexpected answer from peer: {}", magic_enum::enum_name(e)
            ));
        });

        _have_piece_idx = msg->index;
    }
    else if (response->id != proto::MsgId::Unchoke) {
        throw std::runtime_error(fmt::format(
          "Peer not ready to transmit data: peer answer {}",
          magic_enum::enum_name(response->id)
        ));
    }
}

auto PieceWorker::_wait_have() -> bool
{
    using namespace std::chrono_literals;

    spdlog::debug("WAIT HAVE");

    auto result =
      net::tcp::read(io, socket, proto::MsgHeader::SIZE_IN_BYTES, 20s);

    if (not result and result.error() == asio::error::operation_aborted) {
        spdlog::debug("NO HAVE");
        return false;
    }
    else if (not result) {
        throw std::runtime_error(
          fmt::format("Connection error: {}", result.error().message())
        );
    }

    auto response = proto::unpack_msg_header(*result).map_error([](auto&& e) {
        throw std::runtime_error(fmt::format(
          "Unexpected answer from peer: {}", magic_enum::enum_name(e)
        ));
    });

    spdlog::debug("Read: {}", magic_enum::enum_name(response->id));

    if (response->id == proto::MsgId::Have) {
        const auto body =
          net::tcp::read(io, socket, response->body_length)
            .map_error([](auto&& e) {
                throw std::runtime_error(
                  fmt::format("Connection error: {}", e.message())
                );
            });

        const auto msg = proto::unpack_have_msg(*body).map_error([](auto&& e) {
            throw std::runtime_error(fmt::format(
              "Unexpected answer from peer: {}", magic_enum::enum_name(e)
            ));
        });

        _have_piece_idx = msg->index;
        return true;
    }
    else if (response->id != proto::MsgId::Unchoke) {
        throw std::runtime_error(fmt::format(
          "Peer not ready to transmit data: peer answer {}",
          magic_enum::enum_name(response->id)
        ));
    }

    return false;
}

void PieceWorker::_check_piece_hash(
  const size_t& piece_idx,
  const std::vector<uint8_t>& piece_hash,
  const proto::PieceMsg& piece_msg
) const
{
    auto sha = SHA1();

    sha.update(std::string_view(
      reinterpret_cast<const char*>(piece_msg.block.data()),
      piece_msg.block.size()
    ));

    const auto calculated_piece_hash = sha.final();
    const auto expected_hash = fmt::format("{:02x}", fmt::join(piece_hash, ""));

    if (expected_hash != calculated_piece_hash) {
        throw std::runtime_error(fmt::format(
          "Bad piece {} hash: expected {}, got {}", piece_idx, expected_hash,
          calculated_piece_hash
        ));
    }
}

}  // namespace torrent::client
