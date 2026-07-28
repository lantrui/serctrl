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
- **加密存储**: 主密码加密敏感字段
- **传输安全**: WebSocket + 应用层加密

## 第三方依赖

详见 [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES)

## 许可证

[MIT](LICENSE)
