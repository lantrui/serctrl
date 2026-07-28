#pragma once

/**
 * @file kdf.hpp
 * @brief 密钥派生函数（Key Derivation Function）
 *
 * 使用 libsodium 的 crypto_kdf 从输入数据和上下文字符串派生密钥。
 * 适用于从共享秘密派生会话密钥。
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "serctrl/crypto/encryptor.hpp"

namespace serctrl::crypto {

/** @brief 盐值长度 */
constexpr size_t SALT_BYTES = 16;
/** @brief 上下文字符串长度（必须为8字节） */
constexpr size_t CONTEXT_BYTES = 8;

/** @brief 16字节盐值类型 */
using Salt = std::array<uint8_t, SALT_BYTES>;

/**
 * @brief 派生密钥（使用随机盐）
 * @param input 输入数据（如共享秘密）
 * @param context 8字节上下文字符串（用于区分不同用途）
 * @return 32字节派生密钥
 */
Key deriveKey(std::span<const uint8_t> input, std::span<const uint8_t, CONTEXT_BYTES> context);

/**
 * @brief 派生密钥（使用指定盐，可复现）
 * @param input 输入数据
 * @param context 8字节上下文字符串
 * @param salt 盐值
 * @param outSalt 输出盐值（与输入相同，方便存储）
 * @return 32字节派生密钥
 */
Key deriveKeyWithSalt(std::span<const uint8_t> input,
                      std::span<const uint8_t, CONTEXT_BYTES> context,
                      const Salt& salt,
                      Salt& outSalt);

}  // namespace serctrl::crypto
