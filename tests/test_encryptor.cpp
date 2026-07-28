#include <catch2/catch_test_macros.hpp>
#include "serctrl/crypto/encryptor.hpp"
#include "serctrl/crypto/kdf.hpp"
#include "serctrl/crypto/sodium_init.hpp"

using namespace serctrl::crypto;

TEST_CASE("Encryptor encrypt/decrypt roundtrip", "[crypto]") {
    SodiumInit::ensure();

    Key key;
    randombytes_buf(key.data(), key.size());

    Encryptor enc(key);

    std::vector<uint8_t> plaintext = {72, 101, 108, 108, 111};  // "Hello"
    auto ciphertext = enc.encrypt(plaintext);
    auto decrypted = enc.decrypt(ciphertext);

    REQUIRE(decrypted == plaintext);
}

TEST_CASE("Encryptor wrong key fails", "[crypto]") {
    SodiumInit::ensure();

    Key key1, key2;
    randombytes_buf(key1.data(), key1.size());
    randombytes_buf(key2.data(), key2.size());

    Encryptor enc1(key1);
    Encryptor enc2(key2);

    std::vector<uint8_t> plaintext = {72, 101, 108, 108, 111};
    auto ciphertext = enc1.encrypt(plaintext);

    REQUIRE_THROWS(enc2.decrypt(ciphertext));
}

TEST_CASE("Key derivation", "[crypto]") {
    SodiumInit::ensure();

    std::vector<uint8_t> input = {1, 2, 3, 4, 5};
    std::array<uint8_t, 8> context = {'s', 'e', 'r', 'c', 't', 'r', 'l', '1'};

    auto key1 = deriveKey(input, context);
    auto key2 = deriveKey(input, context);

    // Keys should be different (random salt)
    REQUIRE(key1 != key2);
}
