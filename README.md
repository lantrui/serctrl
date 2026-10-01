# SerCTRL

基于 WebSocket 的加密远程 Shell —— SSH 的稳定替代方案，专为救援场景设计。

## 为什么选择 SerCTRL？

SSH 在网络抖动时容易断连，SerCTRL 不会：

| 特性 | SSH | SerCTRL |
|------|-----|---------|
| 协议 | TCP 直连 | WebSocket |
| 断线重连 | 手动 | 自动 |
| 加密算法 | AES | XChaCha20-Poly1305 (libsodium) |
| 文件传输 | scp | 内置 |
| 批量传输 | 不支持 | 支持 |
| 热更新 | 不支持 | 支持 |
| 加密存储 | 不支持 | 支持 |

## 快速开始

```bash
# 编译
mkdir build && cd build
cmake ..
make

# 安装
sudo cmake --install .

# 服务器端
serctrld user add admin -p mypassword
serctrld start

# 客户端
serctrl config add myhost admin@server-ip
serctrl myhost
```

## 客户端命令

```bash
serctrl <user@host>                      # 交互式 shell
serctrl <user@host> exec "ls -la"       # 执行单条命令
serctrl <user@host> put a.txt /remote/  # 上传文件
serctrl <user@host> get /remote/a.txt ./ # 下载文件
serctrl config add <name> <user@host>    # 保存主机
serctrl config list                      # 列出主机
serctrl keygen                           # 生成令牌
serctrl update [--version <v>]           # 更新
serctrl rollback                         # 回滚
```

## 服务器端命令

```bash
serctrld start                       # 启动守护进程
serctrld start -P 8080               # 指定端口
serctrld start -r /home/user         # 沙箱根目录
serctrld user add <name> -p <pass>   # 添加用户
serctrld user list                   # 列出用户
serctrld user remove <name>          # 移除用户
```

## 安全特性

- **加密算法**: XChaCha20-Poly1305 (libsodium)
- **密钥派生**: Argon2id (256MB 内存, 4次迭代)
- **会话密钥握手**: 认证时双方各贡献一次性随机数，会话密钥由
  `BLAKE2b(token || serverNonce || clientNonce)`（域分隔 keyed hash）派生——
  每个连接唯一，杜绝静态密钥重放
- **降级防护**: 加密通道建立后拒绝一切明文帧，防止中间人伪造未加密消息
- **凭据存储**: 密码以 Argon2id 哈希落盘（绝不存明文），文件权限 0600，
  原子写入；验证使用 `sodium_memcmp` 时序安全比较，并对无效用户执行
  dummy hash 抵御用户名枚举计时攻击
- **DoS 防护**: 强制执行最大连接数限制，超限以 1013 拒绝新连接
- **加密存储**: 主密码加密敏感字段
- **传输安全**: WebSocket + 应用层加密

> ⚠️ **升级注意**：本版本修复了旧版 auth.json 明文存储密码的漏洞。
> 旧格式记录会被显式拒绝认证，请使用 `serctrld user add` 重新登记所有用户。
> 与旧版客户端混用时，缺少握手随机数的连接将退回仅含服务端随机数的派生路径；
> 建议两端同步升级到本版本。

## 第三方依赖

本项目未引入新的第三方依赖，全部改动复用以下已有库：

| 库 | 许可证 | 引入方式 |
|----|--------|----------|
| libsodium | ISC | 系统安装 (`find_package`) |
| OpenSSL | Apache-2.0 | 系统安装 (`find_package`) |
| zlib | zlib License | 系统安装 (`find_package`) |
| nlohmann/json | MIT | 系统安装 (`find_package`) |
| uWebSockets / µNetworking (usockets) | Apache-2.0 / BSD-2 | vendor (`third_party/`) |
| ixwebsocket | MIT | vendor (`third_party/`) |
| Catch2 | Boost Software License 1.0 | 系统安装（仅测试） |

完整许可证文本详见 [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES)

## 许可证

[MIT](LICENSE)
