/**
 * @file encrypted_store.cpp
 * @brief 加密存储实现
 *
 * 使用主密码（Argon2id 派生）加密敏感配置字段。
 * 字段值使用 XChaCha20-Poly1305 加密。
 *
 * 加密存储文件格式（encrypted_store.json）：
 * {
 *   "has_password": true,
 *   "salt": "hex...",
 *   "fields": {
 *     "api_token": { "ciphertext": "..." }
 *   }
 * }
 *
 * 安全模型：
 * - 主密码通过 Argon2id（256MB, 4次迭代）派生主密钥
 * - 主密钥用于加密/解密各字段
 * - 盐值随机生成，存储在文件中
 */

#include "serctrl/config/encrypted_store.hpp"
#include "serctrl/crypto/argon2.hpp"
#include "serctrl/crypto/sodium_init.hpp"

#include <sodium.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace serctrl::config {

EncryptedStore::EncryptedStore(const std::filesystem::path& storeFile)
    : storeFile_(storeFile) {
    // 构造时加载现有加密数据
    load();
}

void EncryptedStore::setMasterPassword(const std::string& password) {
    // 首次设置：生成随机盐
    if (passwordSalt_.empty()) {
        passwordSalt_.resize(16);
        randombytes_buf(passwordSalt_.data(), passwordSalt_.size());
    }

    // 使用 Argon2id 从密码派生主密钥
    auto hash = crypto::hashPassword(password, passwordSalt_.data());
    masterKey_ = hash.key;
    hasPassword_ = true;
    save();
}

bool EncryptedStore::hasMasterPassword() const {
    return hasPassword_;
}

bool EncryptedStore::verifyMasterPassword(const std::string& password) const {
    if (!hasPassword_) {
        return false;
    }

    // 重新哈希密码并与存储的密钥比较
    crypto::PasswordHash stored;
    stored.salt = {};
    std::copy(passwordSalt_.begin(), passwordSalt_.end(), stored.salt.begin());
    stored.key = masterKey_;

    return crypto::verifyPassword(password, stored);
}

void EncryptedStore::encryptField(const std::string& key, const std::string& value) {
    if (!hasPassword_) {
        throw std::runtime_error("Master password not set");
    }

    // 使用主密钥加密字段值
    crypto::Encryptor enc(masterKey_);
    auto ciphertext = enc.encrypt(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(value.data()), value.size()));

    EncryptedField field;
    field.ciphertext = std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
    field.nonce.resize(24);  // Nonce 会被 prepend 到密文中

    fields_[key] = std::move(field);
    save();
}

std::optional<std::string> EncryptedStore::decryptField(const std::string& key) const {
    if (!hasPassword_) {
        return std::nullopt;
    }

    auto it = fields_.find(key);
    if (it == fields_.end()) {
        return std::nullopt;
    }

    try {
        // 解密字段值
        crypto::Encryptor enc(masterKey_);
        auto plaintext = enc.decrypt(it->second.ciphertext);
        return std::string(plaintext.begin(), plaintext.end());
    } catch (...) {
        // 解密失败（密码错误或数据损坏）
        return std::nullopt;
    }
}

void EncryptedStore::load() {
    if (!std::filesystem::exists(storeFile_)) {
        return;
    }

    try {
        std::ifstream file(storeFile_);
        nlohmann::json j;
        file >> j;

        // 加载密码状态
        hasPassword_ = j.value("has_password", false);
        
        if (hasPassword_) {
            // 加载盐值（十六进制字符串）
            auto saltHex = j["salt"].get<std::string>();
            passwordSalt_.resize(saltHex.size() / 2);
            for (size_t i = 0; i < passwordSalt_.size(); ++i) {
                unsigned int byte;
                sscanf(saltHex.c_str() + i * 2, "%02x", &byte);
                passwordSalt_[i] = static_cast<uint8_t>(byte);
            }
        }

        // 加载加密字段
        if (j.contains("fields")) {
            for (const auto& [key, field] : j["fields"].items()) {
                EncryptedField ef;
                auto ctB64 = field["ciphertext"].get<std::string>();
                ef.ciphertext.assign(ctB64.begin(), ctB64.end());
                fields_[key] = std::move(ef);
            }
        }
    } catch (...) {
        // 解析错误时忽略
    }
}

void EncryptedStore::save() const {
    // 确保目录存在
    std::filesystem::create_directories(storeFile_.parent_path());

    nlohmann::json j;
    j["has_password"] = hasPassword_;

    if (hasPassword_) {
        // 将盐值转换为十六进制字符串
        std::string saltHex;
        saltHex.reserve(passwordSalt_.size() * 2);
        for (uint8_t byte : passwordSalt_) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            saltHex += buf;
        }
        j["salt"] = saltHex;
    }

    // 序列化加密字段
    j["fields"] = nlohmann::json::object();
    for (const auto& [key, field] : fields_) {
        nlohmann::json f;
        f["ciphertext"] = std::string(field.ciphertext.begin(), field.ciphertext.end());
        j["fields"][key] = f;
    }

    // 写入格式化的 JSON
    std::ofstream file(storeFile_);
    file << j.dump(2);
}

}  // namespace serctrl::config
