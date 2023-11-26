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

auto Metainfo::from_file(std::filesystem::path file_path, bool strict)
  -> std::optional<Metainfo>
{
    auto metainfo_json = metainfo(file_path);
    if (not metainfo_json) {
        return std::nullopt;
    }

    if (strict) {
        bool is_full_meta =
          metainfo_json->contains("announce") and
          metainfo_json->contains("info") and
          metainfo_json->value("info", bencode::Json()).contains("length");

        if (not is_full_meta) {
            return std::nullopt;
        }
    }

    std::string announce = metainfo_json->value("announce", "unknown");

    auto length = metainfo_json->value("info", bencode::Json())
                    .value<bencode::Integer>("length", 0);

    return Metainfo{
      .raw = *metainfo_json, .announce = announce, .length = length
    };
}

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
