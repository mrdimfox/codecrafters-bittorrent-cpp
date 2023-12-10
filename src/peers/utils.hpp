#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace torrent::peers::utils {

inline auto pack_u32(uint32_t value) -> std::vector<uint8_t>
{
    std::vector<uint8_t> packed;
    packed.resize(4);
    if constexpr (std::endian::native == std::endian::big) {
        assert(false);
    }
    else if constexpr (std::endian::native == std::endian::little) {
        packed[0] = (unsigned)value << 24 & 0xFF;
        packed[1] = (unsigned)value << 16 & 0xFF;
        packed[2] = (unsigned)value << 8 & 0xFF;
        packed[3] = (unsigned)value << 0 & 0xFF;
    }

    return packed;
}

inline auto unpack_u32(std::span<uint8_t> msg) -> uint32_t
{
    return (msg[3] << 0) | (msg[2] << 8) | (msg[1] << 16) |
           ((unsigned)msg[0] << 24);
}

}  // namespace torrent::peers::utils
