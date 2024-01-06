#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

#include "bencode/types.hpp"

namespace torrent {

struct Metainfo
{
    const nlohmann::json raw;
    const std::string announce;
    const bencode::Integer length;
    const bencode::Integer piece_length;

    static auto from_file(std::filesystem::path, bool strict = true)
      -> std::optional<Metainfo>;

    auto hash() const -> std::string;
    auto pieces() const -> std::vector<std::vector<uint8_t>>;
};

auto metainfo(std::filesystem::path) -> std::optional<nlohmann::json>;

}  // namespace torrent
