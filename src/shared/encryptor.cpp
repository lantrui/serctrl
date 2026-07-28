/**
 * @file encryptor.cpp
 * @brief 对称加密实现（XChaCha20-Poly1305）
 *
 * 使用 libsodium 的 crypto_secretbox 进行 AEAD 加密。
 * 每次加密生成随机 nonce，密文格式：[nonce][密文+MAC]。
 */

#include "serctrl/crypto/encryptor.hpp"
#include "serctrl/crypto/sodium_init.hpp"

#include <sodium.h>
#include <stdexcept>

namespace serctrl::crypto {

Encryptor::Encryptor(const Key& key) : key_(key) {
    // 确保 libsodium 已初始化
    SodiumInit::ensure();
}

std::vector<uint8_t> Encryptor::encrypt(std::span<const uint8_t> plaintext) const {
    // 生成随机 nonce（每次加密都不同，保证安全性）
    Nonce nonce;
    randombytes_buf(nonce.data(), nonce.size());

    // 输出缓冲区：密文 = 明文 + MAC（认证标签）
    std::vector<uint8_t> ciphertext(plaintext.size() + MAC_BYTES);
    std::vector<uint8_t> nonceVec(nonce.begin(), nonce.end());

    // 执行加密：crypto_secretbox_easy 使用 XChaCha20-Poly1305
    if (crypto_secretbox_easy(ciphertext.data(),
                              plaintext.data(),
                              plaintext.size(),
                              nonce.data(),
                              key_.data()) != 0) {
        throw std::runtime_error("Encryption failed");
    }

    // 最终格式：[24字节nonce][密文+16字节MAC]
    std::vector<uint8_t> result;
    result.reserve(NONCE_BYTES + ciphertext.size());
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());

    return result;
}

std::vector<uint8_t> Encryptor::decrypt(std::span<const uint8_t> data) const {
    // 验证最短长度：nonce + MAC
    if (data.size() < NONCE_BYTES + MAC_BYTES) {
        throw std::runtime_error("Ciphertext too short");
    }

    // 分离 nonce 和密文
    auto nonce = data.subspan(0, NONCE_BYTES);
    auto ciphertext = data.subspan(NONCE_BYTES);

    // 明文长度 = 密文长度 - MAC 长度
    std::vector<uint8_t> plaintext(ciphertext.size() - MAC_BYTES);

    // 执行解密：如果密文被篡改或密钥错误，返回非零
    if (crypto_secretbox_open_easy(plaintext.data(),
                                   ciphertext.data(),
                                   ciphertext.size(),
                                   nonce.data(),
                                   key_.data()) != 0) {
        throw std::runtime_error("Decryption failed");
    }

    return plaintext;
}

}  // namespace serctrl::crypto
