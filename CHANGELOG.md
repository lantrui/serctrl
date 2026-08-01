# Changelog

All notable changes to SerCTRL will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.1.1] - 2026-07-28

### Added
- 传输层加密：认证后所有消息用 XChaCha20-Poly1305 加密
- 交互式 Shell：支持 PTY 会话，ShellData/ShellResize/ShellClose 双向通信
- 第三方依赖 vendor（zero-network build）
- 国构建支持：cmake --install 安装到 /usr/local/bin

### Changed
- 项目归属更新为 lantrui/serctrl
- 客户端自动在认证后启用加密
- 服务端每个连接维护独立的 Enforce

### Fixed
- 修复 cmake 构建网络依赖，引入 local vendor

## [0.1.0] - 2026-07-06

### Added
- Initial release of SerCTRL (C++ rewrite)
- WebSocket-based encrypted remote shell
- XChaCha20-Poly1305 encryption via libsodium
- SSH-style CLI (`serctrl user@host`)
- Server daemon with user management (`serctrld`)
- File upload/download with chunking
- Batch file transfer with SHA-256 verification
- PTY support via POSIX pty
- Auto-reconnect with exponential backoff
- Hot update from GitHub Releases with version selection
- Rollback mechanism
- Encrypted config storage with master password (Argon2id)
- Graceful shutdown with signal handling
- Configurable sandbox root directory
- Max connections limit
- Unit tests with Catch2

### Security
- All data encrypted at rest with libsodium
- Master password encryption for sensitive config fields
- Token verification with timing-safe comparison
- Path traversal prevention
