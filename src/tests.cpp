#include <cassert>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>

#include <asio.hpp>
#include <asio/completion_condition.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "bencode/decoders.hpp"
#include "bencode/encoders.hpp"
#include "bencode/types.hpp"
#include "misc/tcp_transfer.hpp"
#include "proto/deserialize.hpp"
#include "proto/serialize.hpp"
#include "proto/types.hpp"

#include "proto/utils.hpp"
#include "torrent.hpp"

using namespace bencode;
using namespace bencode::internal;
using namespace std::string_view_literals;

void test_decoding();
void test_encoding();
void test_asio();
void test_tcp_transfer();
void test_pack_msg_id();
void test_pack_u32();

void tests()
{
    test_decoding();
    test_encoding();
    // test_asio();
    test_pack_msg_id();
    test_pack_u32();
    // test_tcp_transfer();

    spdlog::debug("All tests passed");
}

void test_pack_u32()
{
    uint32_t a = 32768;
    auto packed = torrent::proto::utils::pack_u32(a);
    auto unpacked = torrent::proto::utils::unpack_u32(packed);
    assert(unpacked == a);
}

void test_pack_msg_id()
{
    using namespace torrent::proto;

    auto id = MsgId::Piece;
    auto packed = torrent::proto::internal::pack_msg_header(id, 9);
    auto unpacked = unpack_msg_header(packed);
    assert(unpacked);

    auto header = *unpacked;
    assert(unpacked->id == id);
    assert(unpacked->body_length == 8);
}

void test_tcp_transfer()
{
    using namespace torrent;
    using namespace asio::ip;
    using namespace std::chrono_literals;

    asio::io_context io;
    tcp::endpoint endpoint(make_address("178.62.82.89"), 51470);
    tcp::socket socket(io);
    socket.connect(endpoint);

    auto meta = Metainfo::from_file("./sample.torrent");
    assert(meta.has_value());

    {
        assert(socket.available() == 0);

        auto handshake_msg = proto::pack_handshake(proto::PeerHandshakeMsg{
          .info_hash = meta->hash(), .peer_id = "00112233445566778899"
        });

        auto result =
          net::tcp::exchange(io, socket, handshake_msg, handshake_msg.size());

        assert(result);

        auto answer = proto::unpack_handshake(*result);
        spdlog::debug("Info hash: {}", answer.info_hash);

        assert(answer.info_hash == meta->hash());
    }

    {
        auto result =
          net::tcp::read(io, socket, proto::MsgHeader::SIZE_IN_BYTES);

        if (result) {
            auto answer = proto::unpack_msg_header(*result);
            assert(answer);
            spdlog::debug("Read: {}", magic_enum::enum_name(answer->id));
            if (answer->id == proto::MsgId::Bitfield) {
                net::tcp::read(io, socket, answer->body_length);
            }
        }
    }

    {
        auto interested_msg = proto::pack_interested_msg();

        auto result = net::tcp::exchange(
          io, socket, interested_msg, proto::MsgHeader::SIZE_IN_BYTES
        );

        assert(result.has_value());

        auto response = proto::unpack_msg_header(*result);
        assert(response);

        spdlog::debug("Answer: {}", magic_enum::enum_name(response->id));
    }

    socket.shutdown(asio::ip::tcp::socket::shutdown_receive);
    io.stop();
}

void test_asio()
{
    using namespace torrent;
    using namespace asio::ip;
    using namespace std::chrono_literals;

    asio::io_context io;

    auto meta = Metainfo::from_file("./sample.torrent");
    assert(meta.has_value());

    auto msg = proto::pack_handshake(proto::PeerHandshakeMsg{
      .info_hash = meta->hash(), .peer_id = "00112233445566778899"
    });

    tcp::endpoint endpoint(make_address("165.232.33.77"), 51467);

    // Connect
    tcp::socket socket(io);
    socket.connect(endpoint);

    // Write timeout timer
    asio::steady_timer write_timeout(io, 1s);
    auto on_write_timeout = [&socket](auto ec) {
        spdlog::debug("Write timer timeout with code: {}", ec.message());

        if (ec) {  // cancel socket on timer cancel
            spdlog::debug("Read timer canceled");
            socket.cancel();
        }
    };

    // Start write timer right away
    write_timeout.async_wait(on_write_timeout);

    // Read timeout timer (will be started later)
    asio::steady_timer read_timeout(io, 1s);
    auto on_read_timeout = [&socket](auto ec) {
        spdlog::debug("Read timer timeout with code: {}", ec.message());
        socket.cancel();
    };

    // Add async write
    asio::error_code write_ec;

    asio::async_write(
      socket, asio::const_buffer(asio::buffer(msg)),
      [&](auto ec, size_t bytes_transferred) -> void {
          write_ec = ec;

          if (write_ec) {
              write_timeout.cancel();
          }

          spdlog::debug("Start read timeout after write transaction");
          read_timeout.async_wait(on_read_timeout);
      }
    );

    asio::error_code read_ec;
    std::vector<uint8_t> data;
    data.reserve(msg.size());

    // Read is canceled after timeout anyway
    asio::async_read(
      socket, asio::dynamic_buffer(data), asio::transfer_at_least(msg.size()),
      [&](auto ec, size_t) {
          read_ec = ec;
          spdlog::debug("Read completed");
          read_timeout.cancel();
      }
    );

    io.run();

    spdlog::debug(
      "Write result: {}.\n Read result: {}", write_ec.message(),
      read_ec.message()
    );

    assert(not write_ec);
    assert(not read_ec or read_ec == asio::error::operation_aborted);

    auto answer = proto::unpack_handshake(data);

    spdlog::debug("{}", answer.peer_id);
}

void test_decoding()
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
}


void test_encoding()
{
    // encode_integer
    {
        assert(encode_integer(Json(123)) == "i123e");
        assert(encode_integer(Json(-123)) == "i-123e");
    }

    // encode_string
    {
        assert(encode_string(Json("123")) == "3:123");
        assert(encode_string(Json("asdasdasd")) == "9:asdasdasd");
    }

    // encode_string
    {
        assert(encode_binary(Json::binary({1, 2, 3})) == "3:\1\2\3");
    }

    // encode_dict
    {
        auto encoded_dict = encode_dict(R"({"foo":"bar","buz":2})"_json);

        assert(encoded_dict != std::nullopt);
        assert(*encoded_dict == "d3:buzi2e3:foo3:bare");
    }

    // encode_dict
    {
        auto encoded_dict = encode_list(R"([1, 2, 3])"_json);

        assert(encoded_dict != std::nullopt);
        assert(*encoded_dict == "li1ei2ei3ee");
    }
}
