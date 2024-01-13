#pragma once

#include <string>
#include <vector>

#include "torrent.hpp"
#include "client/download_file.hpp"  // IWYU pragma: export


namespace torrent::client {

auto get_peers(const Metainfo&) -> std::vector<std::string>;

/**
 * @brief Handshake with peer and return peer id
 */
auto peer_handshake(std::string ip, std::string port, const Metainfo&)
  -> std::string;


/**
 * @brief Download piece by its id
 */
auto download_piece(
  const Metainfo& meta,
  std::string peer_ip,
  std::string peer_port,
  std::size_t piece_idx,
  std::basic_ostream<char>& ostream
) -> void;

}  // namespace torrent::client
