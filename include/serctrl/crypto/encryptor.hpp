#pragma once

/**
 * @file encryptor.hpp
 * @brief 对称加密模块（XChaCha20-Poly1305）
 *
 * 使用 libsodium 的 crypto_secretbox 提供 AEAD 加密。
 * 每次加密生成随机 nonce，密文格式：[24字节nonce][密文+MAC]。
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace serctrl::crypto {

/** @brief 密钥长度（256位） */
constexpr size_t KEY_BYTES = 32;
/** @brief Nonce 长度（192位，XChaCha20 特有） */
constexpr size_t NONCE_BYTES = 24;
/** @brief 认证标签（MAC）长度 */
constexpr size_t MAC_BYTES = 16;

/** @brief 32字节密钥类型 */
using Key = std::array<uint8_t, KEY_BYTES>;
/** @brief 24字节随机数类型 */
using Nonce = std::array<uint8_t, NONCE_BYTES>;

/**
 * @brief 对称加密器
 *
 * 封装 libsodium 的 crypto_secretbox（XChaCha20-Poly1305）。
 * 线程安全：每个线程应使用独立的 Encryptor 实例。
 *
 * 用法：
 * @code
 *   Key key;
 *   randombytes_buf(key.data(), key.size());
 *   Encryptor enc(key);
 *   auto cipher = enc.encrypt(plaintext);
 *   auto plain = enc.decrypt(cipher);
 * @endcode
 */
class Encryptor {
public:
    /**
     * @brief 构造加密器
     * @param key 32字节加密密钥
     */
    explicit Encryptor(const Key& key);

    /**
     * @brief 加密数据
     * @param plaintext 明文数据
     * @return 密文 = [24字节nonce][密文+16字节MAC]
     */
    std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext) const;

    /**
     * @brief 解密数据
     * @param ciphertext 密文（格式：[nonce][密文+MAC]）
     * @return 明文数据
     * @throws std::runtime_error 解密失败（密文被篡改或密钥错误）
     */
    std::vector<uint8_t> decrypt(std::span<const uint8_t> ciphertext) const;

    /** @brief 获取当前使用的密钥 */
    const Key& key() const { return key_; }

private:
    Key key_;
};

}  // namespace serctrl::crypto
