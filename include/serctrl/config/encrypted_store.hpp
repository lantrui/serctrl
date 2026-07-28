#pragma once

/**
 * @file encrypted_store.hpp
 * @brief 加密存储模块
 *
 * 使用主密码加密配置中的敏感字段（如 token）。
 * 加密流程：
 * 1. 用户提供主密码
 * 2. Argon2id 派生 32 字节密钥
 * 3. 用该密钥加密/解密字段
 *
 * 存储格式（JSON）：
 * @code
 * {
 *   "has_password": true,
 *   "salt": "hex...",
 *   "fields": {
 *     "myhost.token": { "ciphertext": "..." }
 *   }
 * }
 * @endcode
 */

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "serctrl/crypto/encryptor.hpp"

namespace serctrl::config {

/**
 * @brief 加密字段
 *
 * 存储加密后的数据和随机数。
 */
struct EncryptedField {
    std::vector<uint8_t> ciphertext;  ///< 密文
    std::vector<uint8_t> nonce;       ///< 随机数
};

/**
 * @brief 加密存储管理器
 *
 * 管理敏感配置字段的加密存储。
 * 主密码用于派生加密密钥，不直接存储。
 *
 * 用法：
 * @code
 *   EncryptedStore store("/path/to/encrypted.json");
 *   store.setMasterPassword("my_secret");
 *   store.encryptField("myhost.token", "abc123");
 *   auto token = store.decryptField("myhost.token");
 * @endcode
 */
class EncryptedStore {
public:
    /**
     * @brief 构造加密存储
     * @param storeFile 加密存储文件路径
     */
    explicit EncryptedStore(const std::filesystem::path& storeFile);

    /**
     * @brief 设置主密码
     * @param password 主密码
     *
     * 首次调用生成随机盐，后续调用验证密码。
     */
    void setMasterPassword(const std::string& password);

    /** @brief 是否已设置主密码 */
    bool hasMasterPassword() const;

    /**
     * @brief 验证主密码
     * @param password 待验证的密码
     * @return 密码是否正确
     */
    bool verifyMasterPassword(const std::string& password) const;

    /**
     * @brief 加密字段
     * @param key 字段键名
     * @param value 明文值
     * @throws std::runtime_error 未设置主密码
     */
    void encryptField(const std::string& key, const std::string& value);

    /**
     * @brief 解密字段
     * @param key 字段键名
     * @return 明文值，不存在或解密失败返回 std::nullopt
     */
    std::optional<std::string> decryptField(const std::string& key) const;

    void load();        ///< 从文件加载
    void save() const;  ///< 保存到文件

private:
    /**
     * @brief 从密码派生密钥
     * @param password 主密码
     * @param salt 16字节盐值
     * @return 32字节派生密钥
     */
    crypto::Key deriveKeyFromPassword(const std::string& password,
                                      std::span<const uint8_t, 16> salt) const;

    std::filesystem::path storeFile_;          ///< 存储文件路径
    bool hasPassword_ = false;                 ///< 是否已设置主密码
    std::vector<uint8_t> passwordSalt_;        ///< 密码盐值
    crypto::Key masterKey_{};                  ///< 派生的主密钥
    std::unordered_map<std::string, EncryptedField> fields_;  ///< 加密字段映射
};

}  // namespace serctrl::config
