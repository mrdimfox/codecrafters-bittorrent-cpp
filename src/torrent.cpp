#include "torrent.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <fmt/core.h>
#include <nlohmann/json.hpp>


#include "bencode/decoders.hpp"
#include "bencode/types.hpp"


namespace torrent {

auto metainfo(std::filesystem::path file_path) -> std::optional<nlohmann::json>
{
    auto torrent_content = [&]() {
        std::ifstream torrent_file;
        torrent_file.open(file_path.c_str(), std::iostream::in);

        return std::string(
          (std::istreambuf_iterator<char>(torrent_file)),
          (std::istreambuf_iterator<char>())
        );
    }();

    auto decoded = bencode::decode_bencoded_value(torrent_content);
    if (not decoded) {
        return std::nullopt;
    }

    auto [_, decoded_value] = *decoded;
    return decoded_value;
}

}  // namespace torrent
