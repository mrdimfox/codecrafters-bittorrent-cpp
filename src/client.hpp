#pragma once

#include <string>
#include <vector>

#include "torrent.hpp"

namespace torrent::client {

auto get_peers(const Metainfo&) -> std::vector<std::string>;

/**
 * @brief Handshake with peer and return peer id
 */
auto peer_handshake(std::string ip, std::string port, const Metainfo&)
  -> std::string;

}  // namespace torrent::client
