#include "torrent.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <vector>


#include "bencode/consts.hpp"
#include "bencode/decoders.hpp"
#include "bencode/encoders.hpp"
#include "bencode/types.hpp"
#include "misc/sha1.hpp"


namespace torrent {

auto Metainfo::from_file(std::filesystem::path file_path, bool strict)
  -> std::optional<Metainfo>
{
    auto metainfo_json = metainfo(file_path);
    if (not metainfo_json) {
        return std::nullopt;
    }

    auto info = metainfo_json->value("info", bencode::Json());

    if (strict) {
        bool is_full_meta = metainfo_json->contains("announce") and
                            metainfo_json->contains("info") and
                            info.contains("length") and
                            info.contains("piece length");

        if (not is_full_meta) {
            return std::nullopt;
        }
    }

    std::string announce = metainfo_json->value("announce", "unknown");

    auto length = info.value<bencode::Integer>("length", 0);
    auto piece_length = info.value<bencode::Integer>("piece length", 0);

    return Metainfo{
      .raw = *metainfo_json,
      .announce = announce,
      .length = length,
      .piece_length = piece_length
    };
}

auto Metainfo::hash() const -> std::string
{
    auto info = this->raw.value("info", bencode::Json());

    auto encoded_info = bencode::encode(info);
    if (not encoded_info or info.empty()) {
        throw std::runtime_error(
          "Can not calculate hash because \"info\" is not exist or malformed"
        );
    }

    SHA1 checksum;
    checksum.update(*encoded_info);

    return checksum.final();
}

auto Metainfo::pieces() const -> std::vector<std::vector<uint8_t>>
{
    auto pieces_bytes = this->raw.value("info", bencode::Json())
                          .value("pieces", bencode::Json::binary_t());

    std::vector<std::vector<uint8_t>> pieces;

    for (auto iter = pieces_bytes.begin(); iter != pieces_bytes.end();
         iter += bencode::PIECE_HASH_LENGTH) {
        pieces.push_back({iter, std::next(iter, bencode::PIECE_HASH_LENGTH)});
    }

    return pieces;
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
