#pragma once

/**
 * @file authenticator.hpp
 * @brief 用户认证模块
 *
 * 管理用户账户、密码验证、令牌生成。
 * 用户数据存储在 auth.json 文件中。
 *
 * 认证流程：
 * 1. 客户端发送用户名 + token
 * 2. 服务端验证 token 是否匹配
 * 3. 认证成功后生成加密会话密钥
 */

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "serctrl/crypto/encryptor.hpp"

namespace serctrl::auth {

/** @brief SHA-256 哈希长度 */
constexpr size_t SHA256_BYTES = 32;
/** @brief 令牌长度（256位） */
constexpr size_t TOKEN_BYTES = 32;
/** @brief auth.json 允许的最大文件大小（字节），防止内存耗尽攻击 */
constexpr uintmax_t MAX_AUTH_FILE_BYTES = 1024 * 1024;
/** @brief 握手随机数长度（256位，与令牌同级熵） */
constexpr size_t HANDSHAKE_NONCE_BYTES = 32;

/** @brief 令牌指纹类型（SHA-256 哈希） */
using Fingerprint = std::array<uint8_t, SHA256_BYTES>;
/** @brief 32字节令牌类型 */
using Token = std::array<uint8_t, TOKEN_BYTES>;
/** @brief 握手随机数类型 */
using HandshakeNonce = std::array<uint8_t, HANDSHAKE_NONCE_BYTES>;

/**
 * @brief 用户记录
 *
 * 存储在 auth.json 中的用户信息。
 */
struct UserRecord {
    std::string name;           ///< 用户名
    std::string passwordHash;   ///< 遗留字段（旧明文格式，仅用于识别/兼容加载，验证时一律拒绝）
    std::string saltHex;        ///< Argon2id 盐值（十六进制）
    std::string storedKeyHex;   ///< Argon2id 派生密钥（十六进制）
    Fingerprint fingerprint;    ///< 令牌指纹
};

/**
 * @brief 认证器
 *
 * 负责用户管理和认证验证。
 * 线程安全：所有操作加锁。
 *
 * 用法：
 * @code
 *   Authenticator auth("/path/to/auth.json");
 *   auth.addUser("admin", "password123");
 *   bool ok = auth.verify("admin", "password123");
 * @endcode
 */
class Authenticator {
public:
    /**
     * @brief 构造认证器
     * @param authFile 认证文件路径
     */
    explicit Authenticator(const std::filesystem::path& authFile);

    /**
     * @brief 添加用户
     * @param name 用户名
     * @param password 密码
     * @return 是否成功（false 表示用户已存在）
     */
    bool addUser(const std::string& name, const std::string& password);

    /**
     * @brief 删除用户
     * @param name 用户名
     * @return 是否成功（false 表示用户不存在）
     */
    bool removeUser(const std::string& name);

    /**
     * @brief 验证用户
     * @param name 用户名
     * @param password 密码
     * @return 密码是否正确
     */
    bool verify(const std::string& name, const std::string& password);

    /**
     * @brief 列出所有用户
     * @return 用户名列表
     */
    std::vector<std::string> listUsers() const;

    /**
     * @brief 生成随机令牌
     * @return 32字节随机令牌
     */
    static Token generateToken();

    /**
     * @brief 令牌转十六进制字符串
     * @param token 令牌
     * @return 64字符十六进制字符串
     */
    static std::string tokenToHex(const Token& token);

    /**
     * @brief 十六进制字符串转令牌
     * @param hex 64字符十六进制字符串
     * @return 令牌
     */
    static Token hexToToken(const std::string& hex);

/**
     * @brief 计算令牌指纹
     * @param token 令牌
     * @return SHA-256 指纹
     */
    static Fingerprint computeFingerprint(const Token& token);

    /**
     * @brief 从共享秘密材料派生会话密钥（HKDF-Extract 模式，Blake2b keyed hash）
     * @param secret 秘密字节序列（如 令牌 || 服务端随机数 || 客户端随机数）
     * @return 32字节会话密钥；每次连接都不同，提供会话级密钥隔离与重放防护
     *
     * 历史实现直接把令牌指纹当作会话密钥：同一令牌的每一次连接都使用
     * 完全相同的静态密钥，任何一次密钥泄露都会危及所有历史/未来会话。
     * 现改为混入双方的一次性随机数，使每个会话拥有独立密钥。
     */
    static crypto::Key deriveSessionKey(std::span<const uint8_t> secret);

    /** @brief 生成一次性握手随机数（密码学安全） */
    static HandshakeNonce generateHandshakeNonce();

    /** @brief 握手随机数转 64 字符十六进制字符串（用于协议传输） */
    static std::string nonceToHex(const HandshakeNonce& nonce);

    /** @brief 十六进制字符串转握手随机数；长度/字符非法时返回全零 */
    static HandshakeNonce hexToNonce(const std::string& hex);

private:
    void load();        ///< 从文件加载用户数据
    void save() const;  ///< 保存用户数据到文件

    std::filesystem::path authFile_;                    ///< 认证文件路径
    std::unordered_map<std::string, UserRecord> users_; ///< 用户记录映射表
};

}  // namespace serctrl::auth
