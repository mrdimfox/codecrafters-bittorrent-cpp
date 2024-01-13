#pragma once

#include <ostream>
#include <string>

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/thread_pool.hpp>
#include <vector>

#include "torrent.hpp"

namespace torrent::client {

/**
 * @brief Download full file
 */
auto download_file(
  const Metainfo& meta,
  const std::vector<std::string> peers,
  std::basic_ostream<char>& ostream
) -> void;

}  // namespace torrent::client
