#pragma once

/**
 * @file error.hpp
 * @brief 错误码定义
 *
 * 统一的错误码枚举，按模块分组。
 * 用于错误传播和日志记录。
 */

#include <cstdint>
#include <string>

namespace serctrl {

/**
 * @brief 错误码枚举
 *
 * 按模块分组：
 * - 0-1: 通用
 * - 10-13: 加密
 * - 20-23: 认证
 * - 30-33: 配置
 * - 40-42: Shell
 * - 50-53: 传输
 * - 60-63: 网络
 * - 70-72: 更新
 */
enum class ErrorCode : uint8_t {
    Ok = 0,                 ///< 成功
    Unknown = 1,            ///< 未知错误

    // ── 加密 ──────────────────────────────
    CryptoInitFailed = 10,  ///< 加密库初始化失败
    EncryptionFailed = 11,  ///< 加密失败
    DecryptionFailed = 12,  ///< 解密失败
    KeyDerivationFailed = 13, ///< 密钥派生失败

    // ── 认证 ──────────────────────────────
    AuthFailed = 20,        ///< 认证失败
    UserExists = 21,        ///< 用户已存在
    UserNotFound = 22,      ///< 用户不存在
    TokenInvalid = 23,      ///< 令牌无效

    // ── 配置 ──────────────────────────────
    ConfigReadError = 30,   ///< 配置读取失败
    ConfigWriteError = 31,  ///< 配置写入失败
    ConfigParseError = 32,  ///< 配置解析失败
    HostNotFound = 33,      ///< 主机未找到

    // ── Shell ─────────────────────────────
    PtyError = 40,          ///< PTY 错误
    ShellOpenFailed = 41,   ///< Shell 打开失败
    ShellClosed = 42,       ///< Shell 已关闭

    // ── 传输 ──────────────────────────────
    FileNotFoundError = 50, ///< 文件不存在
    PermissionDenied = 51,  ///< 权限不足
    TransferFailed = 52,    ///< 传输失败
    ChecksumMismatch = 53,  ///< 校验和不匹配

    // ── 网络 ──────────────────────────────
    ConnectionFailed = 60,  ///< 连接失败
    ConnectionClosed = 61,  ///< 连接已关闭
    Timeout = 62,           ///< 超时
    ProtocolError = 63,     ///< 协议错误

    // ── 更新 ──────────────────────────────
    UpdateFailed = 70,      ///< 更新失败
    VersionMismatch = 71,   ///< 版本不匹配
    RollbackFailed = 72,    ///< 回滚失败
};

/**
 * @brief 获取错误码描述
 * @param code 错误码
 * @return 人类可读的错误描述
 */
std::string errorMessage(ErrorCode code);

}  // namespace serctrl
