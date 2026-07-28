# Changelog

All notable changes to SerCTRL will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
