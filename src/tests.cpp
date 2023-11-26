#include <cassert>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "bencode/decoders.hpp"
#include "bencode/types.hpp"

using namespace bencode;
using namespace bencode::internal;
using namespace std::string_view_literals;


void tests()
{
    // decode_string
    {
        auto res = decode_string("3:abc");
        assert(res != std::nullopt);

        auto [encoded, decoded] = *res;
        assert(encoded.value == "3:abc"sv);
        assert(decoded == Json("abc"));
    }

    {
        auto res = decode_string("3:foo3:bar");
        assert(res != std::nullopt);

        auto [encoded, decoded] = *res;
        assert(encoded.value == "3:foo"sv);
        assert(decoded == Json("foo"));
    }

    {
        auto res = decode_string("3+abc");
        assert(res == std::nullopt);
    }

    // decode_integer
    {
        auto res = decode_integer("i-123e");
        assert(res != std::nullopt);

        auto [encoded, decoded] = *res;
        assert(encoded.value == "i-123e"sv);
        assert(decoded == Json(-123));
    }

    {
        auto res = decode_integer("i100ei-123e");
        assert(res != std::nullopt);

        auto [encoded, decoded] = *res;
        assert(encoded.value == "i100e"sv);
        assert(decoded == Json(100));
    }

    {
        auto res = decode_integer("iasde");
        assert(res == std::nullopt);
    }

    // decode_bencoded_list
    {
        auto res = decode_bencoded_list("l2:abe");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == "l2:abe"sv);

        auto expected_result = Json(std::vector{Json("ab")});
        assert(result.dump() == expected_result.dump());
    }

    {
        auto res = decode_bencoded_list("li123el2:abee");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == "li123el2:abee"sv);

        auto expected_result =
          Json(std::vector{Json(123), Json(std::vector{Json("ab")})});

        assert(result.dump() == expected_result.dump());
    }

    {
        auto res = decode_bencoded_list("l2:aasdasdbe");
        assert(res == std::nullopt);
    }

    // decode_bencoded_dict
    {
        auto res = decode_bencoded_dict("d3:foo3:bar5:helloi52ee");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == "d3:foo3:bar5:helloi52ee"sv);

        auto expected_result = R"({"hello": 52, "foo":"bar"})"_json;
        assert(result.dump() == expected_result.dump());
    }

    {  // test dict is sorted by key
        auto res = decode_bencoded_dict("d1:b3:foo1:a3:bare");
        assert(res != std::nullopt);

        auto [src, result] = *res;

        auto expected_result = R"({"a": "bar", "b":"foo"})"_json;
        assert(result.dump() == expected_result.dump());
    }

    {
        auto res = decode_bencoded_dict("d3:fooee");
        assert(res == std::nullopt);
    }

    {
        auto res = decode_bencoded_dict("d3:foo2bare");
        assert(res == std::nullopt);
    }

    fmt::println("[DEBUG] All tests passed\n");
}
