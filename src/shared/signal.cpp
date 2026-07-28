/**
 * @file signal.cpp
 * @brief 信号处理实现
 *
 * 使用 POSIX sigaction 设置信号处理器。
 * 支持的信号：SIGINT、SIGTERM、SIGQUIT
 *
 * 使用方式：
 * 1. 安装信号处理器：SignalHandler::install(handler)
 * 2. 恢复默认处理：SignalHandler::uninstall()
 */

#include "serctrl/util/signal.hpp"

#include <csignal>
#include <cstdlib>

namespace serctrl {

SignalHandler::Handler SignalHandler::handler_ = nullptr;

void SignalHandler::install(Handler handler) {
    handler_ = std::move(handler);

    // 设置信号处理函数
    struct sigaction sa;
    sa.sa_handler = [](int sig) {
        if (handler_) {
            handler_(sig);
        }
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // 注册信号处理器
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
}

void SignalHandler::uninstall() {
    // 恢复默认信号处理
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    handler_ = nullptr;
}

}  // namespace serctrl
