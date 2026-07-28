#pragma once

/**
 * @file sodium_init.hpp
 * @brief libsodium 初始化工具
 *
 * libsodium 要求在使用任何加密函数前调用 sodium_init()。
 * 这个类提供一个静态方法 ensure()，多次调用是安全的（sodium_init 本身是幂等的）。
 */

#include <sodium.h>
#include <stdexcept>

namespace serctrl::crypto {

/**
 * @brief libsodium 初始化守护类
 *
 * 用法：在使用加密功能前调用 SodiumInit::ensure()。
 * 不可实例化，只提供静态方法。
 */
class SodiumInit {
public:
    /**
     * @brief 确保 libsodium 已初始化
     * @throws std::runtime_error 如果初始化失败
     */
    static void ensure() {
        if (sodium_init() < 0) {
            throw std::runtime_error("Failed to initialize libsodium");
        }
    }

private:
    SodiumInit() = delete;  // 不可实例化
};

}  // namespace serctrl::crypto
