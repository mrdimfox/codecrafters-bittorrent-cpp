#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include <nlohmann/json.hpp>

namespace bencode {

using Json = nlohmann::json;

enum class EncodedValueType
{
    Integer,
    String,
    List,
    Dictionary,
    Unknown,
};

struct EncodedValue
{
    EncodedValueType type;
    std::string_view value;
};

using Dict = std::map<std::string, Json>;
using DecodedValue = std::tuple<EncodedValue, Json>;

using Integer = long long;

}  // namespace bencode
