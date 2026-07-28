#pragma once

/**
 * @file pty.hpp
 * @brief 伪终端（PTY）模块
 *
 * 封装 POSIX 伪终端接口，提供类 RAII 的 PTY 管理。
 * 用于服务端为每个 Shell 会话创建独立的终端环境。
 *
 * 工作原理：
 * 1. 打开主设备（master）
 * 2. fork 子进程
 * 3. 子进程设置会话、绑定从设备（slave）
 * 4. 执行 Shell（如 /bin/sh）
 * 5. 父进程通过 master 与子进程通信
 */

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace serctrl::shell {

/**
 * @brief PTY 配置选项
 *
 * 创建伪终端时的参数。
 */
struct PtyOptions {
    uint32_t cols = 80;             ///< 终端列数
    uint32_t rows = 24;             ///< 终端行数
    std::string command = "/bin/sh"; ///< 要执行的 Shell
    std::vector<std::string> args;  ///< Shell 参数
};

/**
 * @brief 伪终端句柄
 *
 * RAII 封装，析构时自动关闭。
 * 不可复制，可移动。
 *
 * 用法：
 * @code
 *   PtyHandle pty({80, 24, "/bin/bash", {}});
 *   if (pty.isValid()) {
 *       pty.writeData("ls\n");
 *       char buf[1024];
 *       pty.readData(buf);
 *   }
 * @endcode
 */
class PtyHandle {
public:
    /**
     * @brief 创建伪终端
     * @param opts PTY 配置选项
     */
    explicit PtyHandle(const PtyOptions& opts);
    ~PtyHandle();

    // 禁止复制
    PtyHandle(const PtyHandle&) = delete;
    PtyHandle& operator=(const PtyHandle&) = delete;

    // 允许移动
    PtyHandle(PtyHandle&& other) noexcept;
    PtyHandle& operator=(PtyHandle&& other) noexcept;

    /** @brief 获取 master 文件描述符 */
    int fd() const { return fd_; }

    /** @brief PTY 是否有效 */
    bool isValid() const { return fd_ >= 0; }

    /**
     * @brief 调整终端大小
     * @param cols 新列数
     * @param rows 新行数
     * @return 是否成功
     */
    bool resize(uint32_t cols, uint32_t rows);

    /**
     * @brief 写入数据到 PTY
     * @param data 要写入的数据
     * @return 写入的字节数，失败返回 -1
     */
    ssize_t writeData(std::span<const uint8_t> data);

    /**
     * @brief 从 PTY 读取数据
     * @param data 读取缓冲区
     * @return 读取的字节数，失败返回 -1
     */
    ssize_t readData(std::span<uint8_t> data);

    /** @brief 关闭文件描述符 */
    void closeFd();

private:
    int fd_ = -1;       ///< slave 文件描述符（子进程 stdio）
    int masterFd_ = -1; ///< master 文件描述符（父进程通信）
};

}  // namespace serctrl::shell
