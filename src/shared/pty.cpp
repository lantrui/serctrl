/**
 * @file pty.cpp
 * @brief 伪终端（PTY）处理实现
 *
 * 使用 POSIX PTY API 创建伪终端，fork 子进程运行 shell。
 *
 * 工作流程：
 * 1. 调用 posix_openpt() 获取主设备
 * 2. grantpt() + unlockpt() 授权并解锁从设备
 * 3. fork() 创建子进程
 * 4. 子进程：setsid() + 打开从设备 + 重定向 stdin/stdout/stderr
 * 5. 子进程：设置终端大小 + 执行 shell
 * 6. 父进程：持有主设备文件描述符，用于读写
 */

#include "serctrl/shell/pty.hpp"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>

namespace serctrl::shell {

PtyHandle::PtyHandle(const PtyOptions& opts) {
    // 获取伪终端主设备
    int masterFd = posix_openpt(O_RDWR | O_NOCTTY);
    if (masterFd < 0) {
        return;
    }

    // 授权并解锁从设备
    if (grantpt(masterFd) < 0 || unlockpt(masterFd) < 0) {
        ::close(masterFd);
        return;
    }

    // 获取从设备名称
    char* slaveName = ptsname(masterFd);
    if (!slaveName) {
        ::close(masterFd);
        return;
    }

    // fork 子进程
    pid_t pid = fork();
    if (pid < 0) {
        ::close(masterFd);
        return;
    }

    if (pid == 0) {
        // ===== 子进程 =====
        // 创建新的会话，成为会话 leader
        setsid();
        ::close(masterFd);  // 子进程不需要主设备

        // 打开从设备
        int slaveFd = open(slaveName, O_RDWR);
        if (slaveFd < 0) {
            _exit(1);
        }

        // 重定向标准输入/输出/错误到从设备
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);
        if (slaveFd > STDERR_FILENO) {
            ::close(slaveFd);
        }

        // 设置终端大小
        struct winsize ws;
        ws.ws_row = opts.rows;
        ws.ws_col = opts.cols;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(STDIN_FILENO, TIOCSWINSZ, &ws);

        // 重置信号处理为默认
        signal(SIGCHLD, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        // 执行用户的 shell
        const char* shell = getenv("SHELL");
        if (!shell) {
            shell = "/bin/sh";
        }

        execlp(shell, shell, nullptr);
        _exit(1);  // exec 失败
    }

    // ===== 父进程 =====
    // 保存主设备文件描述符
    masterFd_ = masterFd;
    fd_ = masterFd;
}

PtyHandle::~PtyHandle() {
    closeFd();
}

PtyHandle::PtyHandle(PtyHandle&& other) noexcept
    : fd_(other.fd_)
    , masterFd_(other.masterFd_) {
    // 移动语义：转移所有权
    other.fd_ = -1;
    other.masterFd_ = -1;
}

PtyHandle& PtyHandle::operator=(PtyHandle&& other) noexcept {
    if (this != &other) {
        closeFd();
        fd_ = other.fd_;
        masterFd_ = other.masterFd_;
        other.fd_ = -1;
        other.masterFd_ = -1;
    }
    return *this;
}

bool PtyHandle::resize(uint32_t cols, uint32_t rows) {
    if (fd_ < 0) {
        return false;
    }

    // 使用 ioctl 设置终端大小
    struct winsize ws;
    ws.ws_row = rows;
    ws.ws_col = cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    return ioctl(fd_, TIOCSWINSZ, &ws) == 0;
}

ssize_t PtyHandle::writeData(std::span<const uint8_t> data) {
    if (fd_ < 0) {
        return -1;
    }
    // 写入数据到主设备（子进程的从设备会收到）
    return ::write(fd_, data.data(), data.size());
}

ssize_t PtyHandle::readData(std::span<uint8_t> data) {
    if (fd_ < 0) {
        return -1;
    }
    // 从主设备读取数据（子进程的输出）
    return ::read(fd_, data.data(), data.size());
}

void PtyHandle::closeFd() {
    // 关闭文件描述符
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (masterFd_ >= 0 && masterFd_ != fd_) {
        ::close(masterFd_);
        masterFd_ = -1;
    }
}

}  // namespace serctrl::shell
