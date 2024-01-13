#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace torrent::proto::utils {

inline auto pack_u32(uint32_t value) -> std::vector<uint8_t>
{
    std::vector<uint8_t> packed;
    packed.resize(4);
    if constexpr (std::endian::native == std::endian::big) {
        assert(false);
    }
    else if constexpr (std::endian::native == std::endian::little) {
        packed[0] = (value >> 24) & 0xFF;  // Most significant byte
        packed[1] = (value >> 16) & 0xFF;
        packed[2] = (value >> 8) & 0xFF;
        packed[3] = value & 0xFF;          // Least significant byte
    }

    return packed;
}

inline auto unpack_u32(std::span<const uint8_t> msg) -> uint32_t
{
    return (uint32_t)msg[0] << 24 | ((uint32_t)msg[1] << 16) |
           ((uint32_t)msg[2] << 8) | ((uint32_t)msg[3]);
}

}  // namespace torrent::proto::utils
