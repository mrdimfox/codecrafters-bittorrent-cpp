#pragma once

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/thread_pool.hpp>

#include "proto/types.hpp"
#include "torrent.hpp"

namespace torrent::client {

class PieceWorker
{
 public:
    using ProgressCb = std::function<void(
      std::size_t /* piece_idx */,
      std::size_t /* downloaded */,
      std::size_t /* bytes_overall */
    )>;

    inline PieceWorker(
      const Metainfo& meta,
      std::string peer_ip,
      std::size_t peer_port,
      ProgressCb progress_cb = [](std::size_t, std::size_t, std::size_t) {}
    ) :
      meta(meta),
      peer_ip(peer_ip),
      peer_port(peer_port),
      endpoint(asio::ip::make_address(peer_ip), peer_port),
      socket(io),
      _owned_ostream(&_buffer),
      _ostream(_owned_ostream),
      _progress_callback(progress_cb)
    {
    }

    inline PieceWorker(
      const Metainfo& meta,
      std::string peer_ip,
      std::size_t peer_port,
      std::basic_ostream<char>& stream,
      ProgressCb progress_cb = [](std::size_t, std::size_t, std::size_t) {}
    ) :
      meta(meta),
      peer_ip(peer_ip),
      peer_port(peer_port),
      endpoint(asio::ip::make_address(peer_ip), peer_port),
      socket(io),
      _owned_ostream(&_buffer),
      _ostream(stream),
      _progress_callback(progress_cb)
    {
    }

    inline ~PieceWorker()
    {
        if (thread.joinable()) {
            thread.join();
        }

        socket.close();
        io.stop();
    }

    inline void download_piece_async(size_t index)
    {
        if (thread.joinable()) {
            throw std::runtime_error("Last started thread must be awaited.");
        }

        is_piece_transfer_complete = false;
        thread = std::jthread([this, index] {
            try {
                _download_piece(index);
                is_piece_transfer_complete = true;
            } catch (std::exception e) {
                _exception = std::current_exception();
            }
        });
    }

    inline void check_connection_async()
    {
        if (thread.joinable()) {
            throw std::runtime_error("Last started thread must be awaited.");
        }

        thread = std::jthread([this] {
            try {
                _connect_to_peer();
            } catch (std::exception e) {
                _exception = std::current_exception();
            }
        });
    }

    inline bool wait_piece_transfer()
    {
        if (not thread.joinable()) {
            throw std::runtime_error("Thread is not joinable");
        }

        thread.join();
        return is_piece_transfer_complete;
    }

    inline bool wait_connection_established()
    {
        if (not thread.joinable()) {
            throw std::runtime_error("Thread is not joinable");
        }

        thread.join();
        return is_peer_connection_established;
    }

    inline bool started() const noexcept { return thread.joinable(); }
    inline bool raise() const { std::rethrow_exception(_exception); }

    /**
     * @brief Return piece data if it is stored internally
     */
    inline std::string_view piece() const
    {
        return {
          reinterpret_cast<const char*>(_buffer.data().data()),
          _buffer.data().size()
        };
    }

    inline std::size_t last_piece_idx() const { return _last_piece_idx; }

    inline auto have_mode() -> bool
    {
        return _have_mode;
    }

 private:
    void _download_piece(size_t piece_idx);

    void _connect_to_peer();
    void _do_handshake();
    void _do_bitfield_or_unchoke();
    void _do_interested();
    void _try_have();
    bool _wait_have();

    void _check_piece_hash(
      const size_t& piece_idx,
      const std::vector<uint8_t>& piece_hash,
      const proto::PieceMsg& piece_msg
    ) const;

    const Metainfo& meta;

    std::string peer_ip;
    std::size_t peer_port;

    asio::io_context io;
    asio::ip::tcp::endpoint endpoint;
    asio::ip::tcp::socket socket;

    std::jthread thread;

    std::exception_ptr _exception;

    std::basic_ostream<char> _owned_ostream;
    std::basic_ostream<char>& _ostream;
    asio::streambuf _buffer;

    bool _have_mode = false;
    bool is_piece_transfer_complete = false;
    bool is_peer_connection_established = false;
    size_t _last_piece_idx = 0;
    std::optional<size_t> _have_piece_idx;

    ProgressCb _progress_callback;
};

}  // namespace torrent::client
