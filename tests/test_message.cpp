#include <catch2/catch_test_macros.hpp>
#include "serctrl/protocol/message.hpp"
#include "serctrl/protocol/codec.hpp"

using namespace serctrl::protocol;

TEST_CASE("Message encode/decode roundtrip", "[protocol]") {
    Message msg;
    msg.type = MsgType::Auth;
    msg.id = 42;
    msg.ts = 1234567890;
    msg.payload = AuthPayload{"admin", "secret-token"};

    auto encoded = encode(msg);
    auto decoded = decode(encoded);

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->type == MsgType::Auth);
    REQUIRE(decoded->id == 42);
    REQUIRE(decoded->ts == 1234567890);

    auto* auth = std::get_if<AuthPayload>(&decoded->payload);
    REQUIRE(auth != nullptr);
    REQUIRE(auth->user == "admin");
    REQUIRE(auth->token == "secret-token");
}

TEST_CASE("Frame encode/decode", "[protocol]") {
    std::vector<uint8_t> nonce(24, 0xAA);
    std::vector<uint8_t> ciphertext(32, 0xBB);

    auto frame = encodeFrame(FRAME_ENCRYPTED, nonce, ciphertext);
    auto decoded = decodeFrame(frame);

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->first == FRAME_ENCRYPTED);
    REQUIRE(decoded->second.size() == 56);  // 24 + 32
}

TEST_CASE("Message decode failure", "[protocol]") {
    std::vector<uint8_t> invalid = {0x01, 0x02, 0x03};
    auto decoded = decode(invalid);
    REQUIRE_FALSE(decoded.has_value());
}
