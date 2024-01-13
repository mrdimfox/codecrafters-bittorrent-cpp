#pragma once

#include "spdlog/logger.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include <system_error>
#include <vector>

#include <asio/completion_condition.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/basic_endpoint.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <tl/expected.hpp>

namespace net::tcp {

/**
 * @brief Do a one TCP transfer (request + response)
 */
class TcpTransfer
{
 public:
    inline TcpTransfer(
      asio::io_context& io,
      asio::ip::tcp::socket& socket,
      std::chrono::seconds read_timeout
    ) :
      _io(io),
      _socket(socket),
      _read_timeout(read_timeout),
      _write_timeout(std::chrono::seconds(2)),
      _read_timeout_timer(_io, _read_timeout),
      _write_timeout_timer(_io, _write_timeout),
      _logger(spdlog::get("internal_logger"))
    {
    }

    inline ~TcpTransfer() { _io.restart(); }

    inline auto operator()(
      std::span<uint8_t> request, size_t expected_response_length
    ) -> tl::expected<std::vector<uint8_t>, asio::error_code>
    {
        return this->do_transfer(request, expected_response_length);
    }

    inline auto do_transfer(
      std::span<uint8_t> request, size_t expected_response_length
    ) -> tl::expected<std::vector<uint8_t>, asio::error_code>
    {
        using namespace asio::ip;
        using namespace std::chrono_literals;

        if (not _socket.is_open()) {
            return tl::make_unexpected(asio::error::not_connected);
        }

        _prepare_transfer(request, expected_response_length);

        _logger->debug("Start transfer.");
        _io.run();

        _logger->debug(
          "Write result: {}. Read result: {}.", _write_error.message(),
          _read_error.message(), _buffer.size()
        );

        if (_write_error) {
            return tl::make_unexpected(_write_error);
        }

        if (_read_error) {
            return tl::make_unexpected(_read_error);
        }

        return _buffer;
    }

    inline auto do_read(size_t expected_response_length)
      -> tl::expected<std::vector<uint8_t>, asio::error_code>
    {
        using namespace asio::ip;
        using namespace std::chrono_literals;

        if (not _socket.is_open()) {
            return tl::make_unexpected(asio::error::not_connected);
        }

        _prepare_read(expected_response_length);

        _logger->debug("Start reading.");
        _io.run();

        _logger->debug("Read result: {0}", _read_error.message());

        if (_read_error) {
            return tl::make_unexpected(_read_error);
        }

        return _buffer;
    }

 private:
    inline auto _prepare_transfer(
      std::span<uint8_t> request, size_t expected_response_length
    ) -> void
    {
        using namespace asio::ip;
        using namespace std::chrono_literals;

        _expected_response_length = expected_response_length;
        _buffer.resize(_expected_response_length);
        _write_error = asio::error::operation_aborted;
        _read_error = asio::error::operation_aborted;

        asio::async_write(
          _socket, asio::const_buffer(asio::buffer(request)),
          [this](auto ec, size_t bytes_transferred) -> void {
              _on_write_complete(ec, bytes_transferred);
          }
        );

        _write_timeout_timer.async_wait([this](auto ec) {
            _on_write_timeout(ec);
        });
    }

    inline auto _prepare_read(size_t expected_response_length) -> void
    {
        _expected_response_length = expected_response_length;
        _buffer.resize(_expected_response_length);
        _read_error = asio::error::operation_aborted;

        _read_timeout_timer.async_wait([this](asio::error_code ec) {
            _on_read_timeout(ec);
        });

        asio::async_read(
          _socket, asio::buffer(_buffer, _expected_response_length),
          [this](auto ec, size_t bytes_read) {
              return _on_read_partly(ec, bytes_read);
          },
          [this](auto ec, size_t bytes_read) {
              _on_read_complete(ec, bytes_read);
          }
        );
    }


    inline auto _on_write_complete(
      asio::error_code ec, size_t bytes_transferred
    ) -> void
    {
        _write_timeout_timer.cancel();

        if (_write_error = ec; _write_error) {
            _logger->error("Write error, transfer interrupted.");
            _socket.cancel();
            return;
        }

        _logger->debug("Writing compete.");

        _read_timeout_timer.async_wait([this](asio::error_code ec) {
            _on_read_timeout(ec);
        });

        _logger->debug("Start reading.");
        asio::async_read(
          _socket, asio::buffer(_buffer, _expected_response_length),
          [this](auto ec, size_t bytes_read) {
              return _on_read_partly(ec, bytes_read);
          },
          [this](auto ec, size_t bytes_read) {
              _on_read_complete(ec, bytes_read);
          }
        );
    }

    inline auto _on_write_timeout(asio::error_code error) -> void
    {
        if (error == asio::error::operation_aborted) {  // timer canceled
            return;
        }

        if (not error) {
            _logger->error("Write timeout expired");
            _socket.cancel();
        }
    }

    inline auto _on_read_partly(asio::error_code error, size_t bytes_read)
      -> size_t
    {
        _read_timeout_timer.cancel();

        if (_read_error = error; _read_error) {
            _socket.cancel();
            return 0;
        }

        _read_timeout_timer.async_wait([this](asio::error_code ec) {
            _on_read_timeout(ec);
        });

        _logger->debug(
          "Read partly completed, bytes read overall: {}", bytes_read
        );

        return _expected_response_length - bytes_read;
    }

    inline auto _on_read_complete(asio::error_code error, size_t bytes_read)
      -> void
    {
        _read_timeout_timer.cancel();

        if (_read_error = error; _read_error) {
            _logger->error(
              "Read finished abnormally: {}", _read_error.message()
            );
            _socket.cancel();
        }

        _logger->debug("Read completed, bytes read: {}", bytes_read);
    }

    inline auto _on_read_timeout(asio::error_code ec) -> void
    {
        if (ec == asio::error::operation_aborted) {
            _logger->debug("Read timer reset.");
        }

        if (!ec) {
            _logger->error("Read timeout expired.");
            _socket.cancel();
        }
    }

    asio::io_context& _io;
    asio::ip::tcp::socket& _socket;

    std::chrono::seconds _read_timeout;
    std::chrono::seconds _write_timeout;
    asio::steady_timer _read_timeout_timer;
    asio::steady_timer _write_timeout_timer;

    asio::error_code _write_error = asio::error::operation_aborted;
    asio::error_code _read_error = asio::error::operation_aborted;

    std::vector<uint8_t> _buffer;
    std::size_t _expected_response_length = 64;

    std::shared_ptr<spdlog::logger> _logger;
};

inline auto exchange(
  asio::io_context& io,
  asio::ip::tcp::socket& socket,
  std::span<uint8_t> request,
  size_t expected_response_length,
  std::chrono::seconds read_timeout = std::chrono::seconds(10)
)
{
    return TcpTransfer(io, socket, read_timeout)
      .do_transfer(request, expected_response_length);
}

inline auto read(
  asio::io_context& io,
  asio::ip::tcp::socket& socket,
  size_t expected_response_length,
  std::chrono::seconds read_timeout = std::chrono::seconds(10)
)
{
    return TcpTransfer(io, socket, read_timeout)
      .do_read(expected_response_length);
}

}  // namespace net::tcp
