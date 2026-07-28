/**
 * @file kdf.cpp
 * @brief 密钥派生函数实现
 *
 * 使用 libsodium 的 crypto_kdf 从输入数据派生密钥。
 * 适用于从共享秘密派生会话密钥。
 *
 * 注意：crypto_kdf_derive_from_key 的输入是主密钥（32字节），
 * 不是任意输入数据。当前实现直接使用随机盐作为主密钥，
 * 实际应用中应先用输入数据生成主密钥。
 */

#include "serctrl/crypto/kdf.hpp"
#include "serctrl/crypto/sodium_init.hpp"

#include <sodium.h>
#include <stdexcept>

namespace serctrl::crypto {

Key deriveKey(std::span<const uint8_t> input, std::span<const uint8_t, CONTEXT_BYTES> context) {
    SodiumInit::ensure();

    Key key;
    Salt salt;
    // 生成随机盐，每次调用产生不同的密钥
    randombytes_buf(salt.data(), salt.size());

    // 从随机盐派生子密钥
    // subkey_id=0: 派生第一个子密钥
    // context: 8字节上下文，用于区分不同用途
    if (crypto_kdf_derive_from_key(key.data(),
                                   key.size(),
                                   0,
                                   reinterpret_cast<const char*>(context.data()),
                                   salt.data()) != 0) {
        throw std::runtime_error("Key derivation failed");
    }

    return key;
}

Key deriveKeyWithSalt(std::span<const uint8_t> input,
                      std::span<const uint8_t, CONTEXT_BYTES> context,
                      const Salt& salt,
                      Salt& outSalt) {
    SodiumInit::ensure();

    // 输出盐值（与输入相同，方便调用方存储）
    outSalt = salt;
    Key key;

    // 使用指定盐派生密钥（可复现：相同输入+盐=相同密钥）
    if (crypto_kdf_derive_from_key(key.data(),
                                   key.size(),
                                   0,
                                   reinterpret_cast<const char*>(context.data()),
                                   salt.data()) != 0) {
        throw std::runtime_error("Key derivation failed");
    }

    return key;
}

}  // namespace serctrl::crypto
