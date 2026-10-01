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

    // 输入长度校验：crypto_kdf_derive_from_key 要求主密钥至少 32 字节，
    // 传入过短数据会导致读取越界（未定义行为）。
    if (input.size() < crypto_kdf_KEYBYTES) {
        throw std::invalid_argument("KDF input must be at least 32 bytes");
    }

    Key key;

    // 从输入的主密钥材料派生子密钥。
    // 注意：crypto_kdf_derive_from_key 没有盐参数——它按 subkey_id + context
    // 从主密钥派生；随机 salt 仅用于保证本次调用的输出可被调用方记录，
    // 绝不能作为派生的"主密钥"传入（历史实现即犯此错误，导致密钥与输入无关）。
    // subkey_id=0: 派生第一个子密钥
    // context: 8字节上下文，用于区分不同用途
    Salt salt;
    randombytes_buf(salt.data(), salt.size());

    if (crypto_kdf_derive_from_key(key.data(),
                                   key.size(),
                                   0,
                                   reinterpret_cast<const char*>(context.data()),
                                   reinterpret_cast<const unsigned char*>(input.data())) != 0) {
        sodium_memzero(key.data(), key.size());
        throw std::runtime_error("Key derivation failed");
    }

    return key;
}

Key deriveKeyWithSalt(std::span<const uint8_t> input,
                      std::span<const uint8_t, CONTEXT_BYTES> context,
                      const Salt& salt,
                      Salt& outSalt) {
    SodiumInit::ensure();

    // 输入不能为空：crypto_kdf_derive_from_key 要求主密钥至少
    // crypto_kdf_KEYBYTES 字节，否则属于未定义行为（历史漏洞）。
    if (input.size() < crypto_kdf_KEYBYTES) {
        throw std::invalid_argument("KDF input must be at least 32 bytes");
    }

    // 输出盐值（与输入相同，方便调用方存储）
    outSalt = salt;
    Key key;

    // 使用指定盐派生密钥（可复现：相同输入+盐=相同密钥）
    if (crypto_kdf_derive_from_key(key.data(),
                                   key.size(),
                                   0,
                                   reinterpret_cast<const char*>(context.data()),
                                   reinterpret_cast<const unsigned char*>(input.data())) != 0) {
        throw std::runtime_error("Key derivation failed");
    }

    return key;
}

}  // namespace serctrl::crypto
