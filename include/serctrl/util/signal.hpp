#pragma once

/**
 * @file signal.hpp
 * @brief 信号处理模块
 *
 * 用于优雅关闭的信号处理器。
 * 捕获 SIGINT、SIGTERM、SIGQUIT，调用用户回调。
 *
 * 用法：
 * @code
 *   SignalHandler::install([](int sig) {
 *       LOG_INFO("Received signal %d, shutting down...", sig);
 *       server.stop();
 *   });
 * @endcode
 */

#include <functional>

namespace serctrl {

/**
 * @brief 信号处理器
 *
 * 静态类，安装和卸载信号处理。
 * 使用 sigaction 实现，比 signal() 更可靠。
 */
class SignalHandler {
public:
    /** @brief 信号处理回调类型 */
    using Handler = std::function<void(int)>;

    /**
     * @brief 安装信号处理器
     * @param handler 收到信号时调用的回调
     *
     * 捕获的信号：
     * - SIGINT: Ctrl+C
     * - SIGTERM: kill 命令
     * - SIGQUIT: Ctrl+\
     */
    static void install(Handler handler);

    /** @brief 卸载信号处理器（恢复默认行为） */
    static void uninstall();

private:
    static Handler handler_;  ///< 当前处理回调
};

}  // namespace serctrl
