#include <catch2/catch_test_macros.hpp>
#include "serctrl/auth/authenticator.hpp"
#include "serctrl/crypto/argon2.hpp"

using namespace serctrl::auth;
using namespace serctrl::crypto;

TEST_CASE("Token generation", "[auth]") {
    auto token1 = Authenticator::generateToken();
    auto token2 = Authenticator::generateToken();

    REQUIRE(token1 != token2);
    REQUIRE(token1.size() == TOKEN_BYTES);
}

TEST_CASE("Token hex conversion", "[auth]") {
    auto token = Authenticator::generateToken();
    auto hex = Authenticator::tokenToHex(token);
    auto restored = Authenticator::hexToToken(hex);

    REQUIRE(token == restored);
}

TEST_CASE("Password hashing", "[auth]") {
    std::string password = "test_password_123";
    auto hash = hashPassword(password);

    REQUIRE(verifyPassword(password, hash));
    REQUIRE_FALSE(verifyPassword("wrong_password", hash));
}
