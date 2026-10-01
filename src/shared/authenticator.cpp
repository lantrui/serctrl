/**
 * @file authenticator.cpp
 * @brief 用户认证实现
 *
 * 管理用户账户、密码验证、令牌生成。
 * 用户数据以 JSON 格式存储在 auth.json 文件中。
 *
 * auth.json 格式：
 * {
 *   "admin": {
 *     "password_hash": "...",
 *     "fingerprint": "hex..."
 *   }
 * }
 */

#include "serctrl/auth/authenticator.hpp"
#include "serctrl/crypto/argon2.hpp"
#include "serctrl/crypto/sodium_init.hpp"

#include <sodium.h>
#include <sys/stat.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace serctrl::auth {

namespace {

/** @brief 将二进制数据编码为十六进制字符串 */
std::string toHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

/** @brief 将十六进制字符串解码为二进制数据 */
std::vector<uint8_t> fromHex(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = digit(hex[i]);
        int lo = digit(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return {};  // 非法十六进制字符
        }
        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

}  // namespace

Authenticator::Authenticator(const std::filesystem::path& authFile)
    : authFile_(authFile) {
    // 构造时加载现有用户数据
    load();
}

bool Authenticator::addUser(const std::string& name, const std::string& password) {
    // 检查用户是否已存在
    if (users_.count(name)) {
        return false;
    }

    UserRecord record;
    record.name = name;
    // 使用 Argon2id 对密码（即认证令牌）进行哈希后存储，绝不落盘明文
    try {
        auto hash = crypto::hashPassword(std::span<const char>(password.data(), password.size()));
        record.saltHex = toHex(hash.salt.data(), hash.salt.size());
        record.storedKeyHex = toHex(hash.key.data(), hash.key.size());
    } catch (const std::exception&) {
        return false;  // 内存不足等导致 KDF 失败
    }
    // 记录令牌指纹（用于展示/关联，不参与验证）
    record.fingerprint = computeFingerprint(generateToken());

    users_[name] = std::move(record);
    save();
    return true;
}

bool Authenticator::removeUser(const std::string& name) {
    auto it = users_.find(name);
    if (it == users_.end()) {
        return false;
    }
    users_.erase(it);
    save();
    return true;
}

bool Authenticator::verify(const std::string& name, const std::string& password) {
    auto it = users_.find(name);
    if (it == users_.end()) {
        // 用户不存在时同样执行一次 KDF，使两种失败路径耗时接近，
        // 防止通过响应时间枚举有效用户名。
        crypto::Key dummy;
        crypto_pwhash(dummy.data(), dummy.size(),
                      password.data(), password.size(),
                      reinterpret_cast<const uint8_t*>(std::string(crypto::ARGON2_SALT_BYTES, '\0').data()),
                      crypto::ARGON2_OPSLIMIT, crypto::ARGON2_MEMLIMIT,
                      crypto_pwhash_ALG_ARGON2ID13);
        sodium_memzero(dummy.data(), dummy.size());
        return false;
    }

    const UserRecord& record = it->second;
    auto saltBytes = fromHex(record.saltHex);
    auto keyBytes = fromHex(record.storedKeyHex);
    if (saltBytes.size() != crypto::ARGON2_SALT_BYTES ||
        keyBytes.size() != crypto::KEY_BYTES) {
        return false;  // 记录损坏或遗留的明文格式（不安全），拒绝认证
    }

    crypto::PasswordHash stored;
    std::copy(saltBytes.begin(), saltBytes.end(), stored.salt.begin());
    std::copy(keyBytes.begin(), keyBytes.end(), stored.key.begin());

    // Argon2id 重新派生 + sodium_memcmp 时间安全比较
    return crypto::verifyPassword(std::span<const char>(password.data(), password.size()), stored);
}

std::vector<std::string> Authenticator::listUsers() const {
    std::vector<std::string> names;
    names.reserve(users_.size());
    for (const auto& [name, _] : users_) {
        names.push_back(name);
    }
    return names;
}

Token Authenticator::generateToken() {
    Token token;
    // 生成密码学安全的随机令牌
    randombytes_buf(token.data(), token.size());
    return token;
}

std::string Authenticator::tokenToHex(const Token& token) {
    // 将二进制令牌转换为十六进制字符串
    std::string hex;
    hex.reserve(token.size() * 2);
    for (uint8_t byte : token) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", byte);
        hex += buf;
    }
    return hex;
}

Token Authenticator::hexToToken(const std::string& hex) {
    // 将十六进制字符串转换为二进制令牌（严格校验，拒绝非法字符/长度）
    Token token{};
    if (hex.size() != token.size() * 2) {
        return token;  // 长度不符返回全零令牌，调用方应自行校验
    }
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < token.size(); ++i) {
        int hi = digit(hex[i * 2]);
        int lo = digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return Token{};  // 含非法字符
        }
        token[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return token;
}

Fingerprint Authenticator::computeFingerprint(const Token& token) {
    // 使用 Blake2b 计算令牌指纹（用于快速比较）
    Fingerprint fp;
    crypto_generichash(fp.data(), fp.size(),
                       token.data(), token.size(),
                       nullptr, 0);
    return fp;
}

HandshakeNonce Authenticator::generateHandshakeNonce() {
    HandshakeNonce nonce;
    randombytes_buf(nonce.data(), nonce.size());
    return nonce;
}

std::string Authenticator::nonceToHex(const HandshakeNonce& nonce) {
    return toHex(nonce.data(), nonce.size());
}

HandshakeNonce Authenticator::hexToNonce(const std::string& hex) {
    HandshakeNonce nonce{};
    if (hex.size() != nonce.size() * 2) {
        return nonce;  // 长度不符返回全零，调用方应自行校验
    }
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < nonce.size(); ++i) {
        int hi = digit(hex[i * 2]);
        int lo = digit(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return HandshakeNonce{};  // 含非法字符
        }
        nonce[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return nonce;
}

crypto::Key Authenticator::deriveSessionKey(std::span<const uint8_t> secret) {
    // HKDF-Extract 风格：以固定域分隔字符串为密钥的 Blake2b keyed hash，
    // 从（令牌 || 服务端随机数 || 客户端随机数）派生一次性会话密钥。
    // 每个连接的随机数不同 ⇒ 会话密钥不同 ⇒ 单会话密钥泄露不危及其他会话，
    // 同时旧会话密文无法被重放到新会话（AEAD MAC 校验失败）。
    static const char kInfo[] = "serctrl-session-key-v1";
    crypto::Key key{};
    crypto_generichash(key.data(), key.size(),
                      secret.data(), secret.size(),
                      reinterpret_cast<const unsigned char*>(kInfo), sizeof(kInfo) - 1);
    return key;
}

void Authenticator::load() {
    // 如果文件不存在，跳过
    if (!std::filesystem::exists(authFile_)) {
        return;
    }

    try {
        // 防御超大文件导致的内存耗尽（DoS）
        std::error_code ec;
        auto fileSize = std::filesystem::file_size(authFile_, ec);
        if (ec || fileSize > MAX_AUTH_FILE_BYTES) {
            return;
        }

        std::ifstream file(authFile_);
        nlohmann::json j;
        file >> j;

        // 解析每个用户
        for (const auto& [name, record] : j.items()) {
            if (!record.is_object()) {
                continue;
            }
            UserRecord ur;
            ur.name = name;
            if (record.contains("password_hash") && record["password_hash"].is_string()) {
                ur.passwordHash = record["password_hash"].get<std::string>();
            }
            if (record.contains("salt") && record["salt"].is_string()) {
                ur.saltHex = record["salt"].get<std::string>();
            }
            if (record.contains("stored_key") && record["stored_key"].is_string()) {
                ur.storedKeyHex = record["stored_key"].get<std::string>();
            }
            // 加载指纹（十六进制字符串，严格校验长度与字符集）
            if (record.contains("fingerprint") && record["fingerprint"].is_string()) {
                auto fpHex = record["fingerprint"].get<std::string>();
                auto fpBytes = fromHex(fpHex);
                if (fpBytes.size() == ur.fingerprint.size()) {
                    std::copy(fpBytes.begin(), fpBytes.end(), ur.fingerprint.begin());
                }
            }
            users_[name] = std::move(ur);
        }
    } catch (...) {
        // 解析错误时忽略，使用空数据
    }
}

void Authenticator::save() const {
    // 确保目录存在，并收紧权限（auth.json 含密钥材料，仅属主可读写）
    std::filesystem::create_directories(authFile_.parent_path());
    std::filesystem::permissions(
        authFile_.parent_path(),
        std::filesystem::perms::owner_all | std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec,
        std::filesystem::perm_options::replace);

    nlohmann::json j;
    for (const auto& [name, record] : users_) {
        nlohmann::json ur;
        // Argon2id 派生结果：盐 + 派生密钥（均为十六进制），绝不写入明文密码
        if (!record.saltHex.empty() && !record.storedKeyHex.empty()) {
            ur["salt"] = record.saltHex;
            ur["stored_key"] = record.storedKeyHex;
        } else {
            // 兼容旧记录（不应出现；verify() 会拒绝这类不安全记录）
            ur["password_hash"] = record.passwordHash;
        }
        ur["fingerprint"] = toHex(record.fingerprint.data(), record.fingerprint.size());
        j[name] = ur;
    }

    // 先写入临时文件再原子重命名，避免进程崩溃导致凭据文件被截断损坏
    const auto tmpPath = authFile_.string() + ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
        file << j.dump(2);
        file.flush();
        if (!file.good()) {
            throw std::runtime_error("Failed to write auth file");
        }
    }
    ::chmod(tmpPath.c_str(), S_IRUSR | S_IWUSR);  // 0600：仅属主可读写
    std::filesystem::rename(tmpPath, authFile_);
}

}  // namespace serctrl::auth
