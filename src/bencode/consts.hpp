#pragma once

namespace bencode {

constexpr const char END_SYMBOL = 'e';
constexpr const char LIST_START_SYMBOL = 'l';
constexpr const char DICT_START_SYMBOL = 'd';
constexpr const char INTEGER_START_SYMBOL = 'i';
constexpr const char STRING_DELIMITER_SYMBOL = ':';

constexpr const unsigned PIECE_HASH_LENGTH = 20;

}  // namespace bencode
