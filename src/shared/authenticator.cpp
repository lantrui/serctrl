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
#include "serctrl/crypto/sodium_init.hpp"

#include <sodium.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace serctrl::auth {

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
    // TODO: 应使用 Argon2id 哈希密码，当前仅存储明文（不安全）
    record.passwordHash = password;
    // 为用户生成随机令牌并计算指纹
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
        return false;
    }
    // TODO: 应使用 verifyPassword() 进行安全验证
    return it->second.passwordHash == password;
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
    // 将十六进制字符串转换为二进制令牌
    Token token{};
    for (size_t i = 0; i < std::min(hex.size() / 2, token.size()); ++i) {
        unsigned int byte;
        sscanf(hex.c_str() + i * 2, "%02x", &byte);
        token[i] = static_cast<uint8_t>(byte);
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

void Authenticator::load() {
    // 如果文件不存在，跳过
    if (!std::filesystem::exists(authFile_)) {
        return;
    }

    try {
        std::ifstream file(authFile_);
        nlohmann::json j;
        file >> j;

        // 解析每个用户
        for (const auto& [name, record] : j.items()) {
            UserRecord ur;
            ur.name = name;
            ur.passwordHash = record["password_hash"];
            // 加载指纹（十六进制字符串）
            if (record.contains("fingerprint")) {
                auto fpHex = record["fingerprint"].get<std::string>();
                for (size_t i = 0; i < std::min(fpHex.size() / 2, ur.fingerprint.size()); ++i) {
                    unsigned int byte;
                    sscanf(fpHex.c_str() + i * 2, "%02x", &byte);
                    ur.fingerprint[i] = static_cast<uint8_t>(byte);
                }
            }
            users_[name] = std::move(ur);
        }
    } catch (...) {
        // 解析错误时忽略，使用空数据
    }
}

void Authenticator::save() const {
    // 确保目录存在
    std::filesystem::create_directories(authFile_.parent_path());

    nlohmann::json j;
    for (const auto& [name, record] : users_) {
        nlohmann::json ur;
        ur["password_hash"] = record.passwordHash;
        // 将指纹转换为十六进制字符串
        std::string fpHex;
        fpHex.reserve(record.fingerprint.size() * 2);
        for (uint8_t byte : record.fingerprint) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            fpHex += buf;
        }
        ur["fingerprint"] = fpHex;
        j[name] = ur;
    }

    // 写入格式化的 JSON
    std::ofstream file(authFile_);
    file << j.dump(2);
}

}  // namespace serctrl::auth
