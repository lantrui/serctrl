/**
 * @file argon2.cpp
 * @brief 密码哈希实现（Argon2id）
 *
 * 使用 libsodium 的 crypto_pwhash 进行密码哈希。
 * Argon2id 是目前最安全的密码哈希算法，抗 GPU/ASIC 攻击。
 *
 * 参数说明：
 * - opsLimit=4: 迭代次数，控制 CPU 耗时
 * - memLimit=256MB: 内存消耗，防止内存硬攻击
 * - ALG_ARGON2ID13: Argon2id v1.3（混合 Argon2i 和 Argon2d）
 */

#include "serctrl/crypto/argon2.hpp"
#include "serctrl/crypto/sodium_init.hpp"

#include <sodium.h>
#include <stdexcept>

namespace serctrl::crypto {

PasswordHash hashPassword(std::span<const char> password, const uint8_t* saltPtr) {
    SodiumInit::ensure();

    PasswordHash result;

    // 使用提供的盐或生成随机盐
    if (saltPtr == nullptr) {
        // 首次哈希：随机生成盐
        randombytes_buf(result.salt.data(), result.salt.size());
    } else {
        // 验证时：使用存储的盐
        std::copy(saltPtr, saltPtr + ARGON2_SALT_BYTES, result.salt.begin());
    }

    // 执行 Argon2id 密码哈希
    // 输出：32字节密钥（可用于加密）
    if (crypto_pwhash(result.key.data(),
                       result.key.size(),
                       password.data(),
                       password.size(),
                       result.salt.data(),
                       ARGON2_OPSLIMIT,
                       ARGON2_MEMLIMIT,
                       crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("Password hashing failed");
    }

    return result;
}

bool verifyPassword(std::span<const char> password, const PasswordHash& stored) {
    SodiumInit::ensure();

    Key derived;

    // 用相同参数重新哈希密码
    if (crypto_pwhash(derived.data(),
                      derived.size(),
                      password.data(),
                      password.size(),
                      stored.salt.data(),
                      ARGON2_OPSLIMIT,
                      ARGON2_MEMLIMIT,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        return false;
    }

    // 使用时间安全比较，防止时序攻击
    return sodium_memcmp(derived.data(), stored.key.data(), stored.key.size()) == 0;
}

}  // namespace serctrl::crypto
