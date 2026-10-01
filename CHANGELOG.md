# Changelog

All notable changes to SerCTRL will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased] - 安全加固

本版本集中修复了代码审计中发现的安全漏洞。所有改动均复用项目内已有的
libsodium 密码学原语，未引入任何新的第三方依赖。

### Added（安全机制）
- **会话密钥握手协议**：认证载荷新增 `clientNonce`、认证结果新增 `serverNonce`
  （各 32 字节随机数）。会话密钥改由 BLAKE2b keyed hash（域分隔
  `"serctrl-session-key-v1"`）对 `token || serverNonce || clientNonce` 派生，
  每个连接的会话密钥唯一，取代历史的"令牌指纹静态密钥"（同一令牌每次
  连接密钥相同，可被重放）。旧对端缺省字段时自动降级为兼容路径。
- **明文帧降级防护**：加密通道建立后，服务端与客户端均拒绝一切明文（TEXT）
  帧并记录告警日志，防止中间位置伪造明文消息绕过加密层。
- **用户名枚举防护**：认证不存在的用户时同样执行一次完整 Argon2id 运算
  （dummy hash），使两条失败路径耗时接近，抵御计时侧信道。
- **凭据文件原子写入**：auth.json 改为 `.tmp` 写入 → `chmod 0600` → 原子
  rename，避免进程崩溃导致凭据损坏或以过宽权限落盘。
- **严格 Hex 解码器**：替换 `sscanf` 静默解析，校验长度与字符集，非法输入
  返回空；同时新增 auth 文件大小上限，防止超大文件耗尽内存。
- **最大连接数强制执行**：连接回调中持锁检查 `maxConnections`，超限以
  WebSocket 状态码 1013 (Try Again Later) 拒绝新连接，封堵 DoS 资源耗尽。
- **干净断开**：`disconnectClient()` 改用 `end(1000)` 发送标准 Close 帧
  （原 `close()` 为裸 TCP 关闭，对端表现为协议错误），且不再双重删除
  socket 映射（由 close 回调统一清理）。

### Fixed（漏洞修复）
- **[严重] 密码明文存储**：`Authenticator::addUser()` 历史实现将密码明文
  写入 auth.json，并用非时序安全的 `==` 比较。现改为 Argon2id 哈希存储 +
  `sodium_memcmp` 验证；加载到遗留明文格式的记录在验证时被显式拒绝。
- **[高危] KDF 密钥派生错误**：`deriveKey()` / `deriveKeyWithSalt()` 历史
  实现误将随机盐作为主密钥传入 `crypto_kdf_derive_from_key`，导致派生的
  密钥与输入完全脱钩（等于没有密钥）。已修正参数传递，并增加输入长度
  ≥ 32 字节的校验（过短输入原本会触发库内越界读，属未定义行为），失败
  路径清零密钥缓冲。
- **[中危] 密钥材料残留**：会话密钥派生用的临时 secret 缓冲区在派生完成后
  立即 `sodium_memzero` 清零。

### Changed
- `Client::enableEncryption()` 标记为 `@deprecated`（静态密钥无会话隔离），
  新代码应使用 `Client::handleAuthResult()`；保留仅为兼容旧版服务端。
- 协议 JSON 编解码改用 `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`，
  可选字段（握手随机数）缺省时不再抛异常。

### Known Limitations（已知限制 / 后续工作）
- 热更新模块（updater）的下载完整性校验（SHA-256）、备份与原子替换仍为
  TODO 占位，发布前必须补齐或禁用该功能。
- exec / shell / 文件传输的业务处理逻辑尚未接入当前版本的 server 事件循环；
  接入时必须：仅在加密状态下放行命令执行、强制执行 `timeout`、并以
  `config.rootDir` 实现路径穿越防护（沙箱）。
- wss:// TLS 证书固定（certificate pinning）待 uWS/ixwebsocket 配置接线后启用。
- 本次改动在无完整工具链的环境下未经编译验证，合并前需 CI 全量构建 +
  Catch2 单测通过。

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
