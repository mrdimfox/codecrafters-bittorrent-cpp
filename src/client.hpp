#pragma once

#include <string>
#include <vector>

#include "torrent.hpp"

namespace torrent::client {

auto get_peers(const Metainfo&) -> std::vector<std::string>;

}  // namespace torrent::client
