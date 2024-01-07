#pragma once

#include <cstddef>
#include <exception>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/thread_pool.hpp>
#include <thread>
#include <vector>

#include "torrent.hpp"

namespace torrent::client {

class PieceWorker
{
 public:
    inline PieceWorker(
      const Metainfo& meta, std::string peer_ip, std::size_t peer_port
    ) :
      meta(meta),
      peer_ip(peer_ip),
      peer_port(peer_port),
      endpoint(asio::ip::make_address(peer_ip), peer_port),
      socket(io),
      _owned_ostream(&_buffer),
      _ostream(_owned_ostream)
    {
    }

    inline PieceWorker(
      const Metainfo& meta,
      std::string peer_ip,
      std::size_t peer_port,
      std::basic_ostream<char>& stream
    ) :
      meta(meta),
      peer_ip(peer_ip),
      peer_port(peer_port),
      endpoint(asio::ip::make_address(peer_ip), peer_port),
      socket(io),
      _owned_ostream(&_buffer),
      _ostream(stream)
    {
    }

    inline ~PieceWorker()
    {
        if (thread.joinable()) {
            thread.join();
        }

        thread_pool.stop();
        thread_pool.join();
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
                exception = std::current_exception();
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
                exception = std::current_exception();
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

    inline bool started()
    {
        return thread.joinable();
    }

    inline bool raise() { std::rethrow_exception(exception); }

    inline const std::string_view piece() { return _buffer.view(); }

 private:
    void _download_piece(size_t piece_idx);

    void _connect_to_peer();
    void _do_handshake();
    void _do_bitfield();
    void _do_interested();

    const Metainfo& meta;

    std::string peer_ip;
    std::size_t peer_port;

    asio::io_context io;
    asio::ip::tcp::endpoint endpoint;
    asio::ip::tcp::socket socket;
    asio::thread_pool thread_pool;

    std::jthread thread;

    std::exception_ptr exception;

    // vecbuf<char> _buffer;
    std::basic_ostream<char> _owned_ostream;
    std::basic_ostream<char>& _ostream;
    std::stringbuf _buffer;

    bool is_piece_transfer_complete = false;
    bool is_peer_connection_established = false;
};

}  // namespace torrent::client
