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


void tests()
{
    // extract_string
    {
        auto res = extract_bencoded_string("3:test");
        assert(res != std::nullopt);
        assert(*res == std::string_view("3:tes"));
    }

    {
        auto res = extract_bencoded_string("4:test");
        assert(res != std::nullopt);
        assert(*res == std::string_view("4:test"));
    }

    {
        auto res = extract_bencoded_string("10:test");
        assert(res == std::nullopt);
    }

    {
        auto res = extract_bencoded_string("10test");
        assert(res == std::nullopt);
    }

    {
        std::string_view encoded_string{"4:test1:a"};
        auto res = extract_bencoded_string(encoded_string);
        assert(res != std::nullopt);
        assert(*res == std::string_view("4:test"));

        auto res2 = extract_bencoded_string(
          std::string_view(res->end(), encoded_string.end())
        );
        assert(res2 != std::nullopt);
        assert(*res2 == std::string_view("1:a"));
    }

    // extract_integer
    {
        auto res = extract_bencoded_integer("i123e");
        assert(res != std::nullopt);
        assert(*res == std::string_view("i123e"));
    }

    {
        auto res = extract_bencoded_integer("i123ee");
        assert(res != std::nullopt);
        assert(*res == std::string_view("i123e"));
    }

    {
        auto res = extract_bencoded_integer("i123");
        assert(res == std::nullopt);
    }

    // extract_bencoded_value
    {
        auto res = extract_bencoded_value("i123e");
        assert(res.type == EncodedValueType::Integer);
        assert(res.value == std::string_view("i123e"));
    }

    {
        auto res = extract_bencoded_value("3:test");
        assert(res.type == EncodedValueType::String);
        assert(res.value == std::string_view("3:tes"));
    }

    {
        auto res = extract_bencoded_value("le");
        assert(res.type == EncodedValueType::List);
        assert(res.value == std::string_view("le"));
    }

    {
        auto res = extract_bencoded_value("abcx");
        assert(res.type == EncodedValueType::Unknown);
    }

    // decode_string
    {
        try {
            auto res = decode_string("3:abc");
            assert(res == Json("abc"));
        } catch (const std::exception& e) {
            assert(false && "No errors expected");
        }
    }

    {
        try {
            auto res = decode_string("3+abc");
            assert(false && "Test should raise");
        } catch (const std::exception& e) {
            auto msg = std::string_view(e.what());
            assert(msg.find("3+abc") != std::string_view::npos);
        }
    }

    // decode_integer
    {
        try {
            auto res = decode_integer("i-123e");
            assert(res == Json(-123));
        } catch (const std::exception& e) {
            assert(false && "No errors expected");
        }
    }

    {
        try {
            auto res = decode_integer("iasde");
            assert(false && "Test should raise");
        } catch (const std::exception& e) {
            auto msg = std::string_view(e.what());
            assert(msg.find("iasde") != std::string_view::npos);
        }
    }

    // decode_bencoded_list
    {
        auto res = decode_bencoded_list("l2:abe");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == std::string_view("l2:abe"));

        auto expected_result = Json(std::vector{Json("ab")});
        assert(result.dump() == expected_result.dump());
    }

    {
        auto res = decode_bencoded_list("li123el2:abee");
        assert(res != std::nullopt);

        auto [src, result] = *res;
        assert(src.value == std::string_view("li123el2:abee"));

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
        assert(src.value == std::string_view("d3:foo3:bar5:helloi52ee"));

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
