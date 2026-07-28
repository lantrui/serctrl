/**
 * @file error.cpp
 * @brief 错误码转换实现
 *
 * 将 ErrorCode 枚举转换为人类可读的错误消息。
 */

#include "serctrl/util/error.hpp"

namespace serctrl {

std::string errorMessage(ErrorCode code) {
    switch (code) {
        // 通用错误
        case ErrorCode::Ok: return "Success";
        case ErrorCode::Unknown: return "Unknown error";

        // 加密错误
        case ErrorCode::CryptoInitFailed: return "Failed to initialize cryptography";
        case ErrorCode::EncryptionFailed: return "Encryption failed";
        case ErrorCode::DecryptionFailed: return "Decryption failed";
        case ErrorCode::KeyDerivationFailed: return "Key derivation failed";

        // 认证错误
        case ErrorCode::AuthFailed: return "Authentication failed";
        case ErrorCode::UserExists: return "User already exists";
        case ErrorCode::UserNotFound: return "User not found";
        case ErrorCode::TokenInvalid: return "Invalid token";

        // 配置错误
        case ErrorCode::ConfigReadError: return "Failed to read configuration";
        case ErrorCode::ConfigWriteError: return "Failed to write configuration";
        case ErrorCode::ConfigParseError: return "Failed to parse configuration";
        case ErrorCode::HostNotFound: return "Host not found";

        // Shell 错误
        case ErrorCode::PtyError: return "PTY error";
        case ErrorCode::ShellOpenFailed: return "Failed to open shell";
        case ErrorCode::ShellClosed: return "Shell closed";

        // 文件传输错误
        case ErrorCode::FileNotFoundError: return "File not found";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::TransferFailed: return "Transfer failed";
        case ErrorCode::ChecksumMismatch: return "Checksum mismatch";

        // 传输层错误
        case ErrorCode::ConnectionFailed: return "Connection failed";
        case ErrorCode::ConnectionClosed: return "Connection closed";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::ProtocolError: return "Protocol error";

        // 更新错误
        case ErrorCode::UpdateFailed: return "Update failed";
        case ErrorCode::VersionMismatch: return "Version mismatch";
        case ErrorCode::RollbackFailed: return "Rollback failed";
    }

    return "Unknown error";
}

}  // namespace serctrl
