#pragma once

/**
 * @file argon2.hpp
 * @brief 密码哈希模块（Argon2id）
 *
 * 使用 Argon2id 算法进行密码哈希，抗 GPU/ASIC 暴力破解。
 * 用于主密码派生和用户密码存储。
 *
 * Argon2id 参数：
 * - opsLimit=4：计算次数，控制 CPU 耗时
 * - memLimit=256MB：内存消耗，防止内存硬攻击
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "serctrl/crypto/encryptor.hpp"

namespace serctrl::crypto {

/** @brief Argon2 盐值长度 */
constexpr size_t ARGON2_SALT_BYTES = 16;
/** @brief 计算次数（影响 CPU 耗时） */
constexpr size_t ARGON2_OPSLIMIT = 4;
/** @brief 内存限制（256MB，防止内存硬攻击） */
constexpr size_t ARGON2_MEMLIMIT = 256 * 1024 * 1024;

/**
 * @brief 密码哈希结果
 *
 * 包含盐值和派生的密钥，存储时需要同时保存两者。
 */
struct PasswordHash {
    std::array<uint8_t, ARGON2_SALT_BYTES> salt;  ///< 盐值（随机生成）
    Key key;                                        ///< 派生的32字节密钥
};

/**
 * @brief 对密码进行哈希
 * @param password 密码
 * @param saltPtr 盐值指针（nullptr 则随机生成）
 * @return 哈希结果（包含盐值和密钥）
 */
PasswordHash hashPassword(std::span<const char> password, const uint8_t* saltPtr = nullptr);

/**
 * @brief 验证密码
 * @param password 待验证的密码
 * @param stored 存储的哈希结果
 * @return 密码是否匹配
 */
bool verifyPassword(std::span<const char> password, const PasswordHash& stored);

}  // namespace serctrl::crypto
