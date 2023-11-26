#pragma once

#include <cstdint>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "bencode/types.hpp"

namespace torrent {

struct Metainfo
{
    const nlohmann::json raw;
    const std::string announce;
    const bencode::Integer length;

    static auto from_file(std::filesystem::path, bool strict = true)
      -> std::optional<Metainfo>;

    auto hash() -> std::uint64_t;
};

auto metainfo(std::filesystem::path) -> std::optional<nlohmann::json>;

}  // namespace torrent
