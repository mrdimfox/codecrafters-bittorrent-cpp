#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

namespace torrent {

auto metainfo(std::filesystem::path) -> std::optional<nlohmann::json>;

}  // namespace torrent
