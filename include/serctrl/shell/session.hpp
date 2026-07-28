#pragma once

/**
 * @file session.hpp
 * @brief Shell 会话管理
 *
 * 管理单个 Shell 会话的生命周期。
 * 每个会话对应一个 PTY 和一个子进程。
 *
 * 会话状态：
 * - 创建 → 运行中 → 关闭
 * - 可通过回调接收数据和退出事件
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "serctrl/shell/pty.hpp"

namespace serctrl::shell {

/**
 * @brief Shell 会话
 *
 * 封装 PTY 和会话状态，提供简单的读写接口。
 *
 * 用法：
 * @code
 *   Session session("sess-1", {80, 24, "/bin/bash", {}});
 *   session.onData([](const std::string& data) {
 *       // 处理终端输出
 *   });
 *   session.write("ls -la\n");
 * @endcode
 */
class Session {
public:
    /**
     * @brief 创建会话
     * @param id 会话ID（唯一标识）
     * @param opts PTY 配置选项
     */
    Session(const std::string& id, const PtyOptions& opts);
    ~Session();

    /** @brief 获取会话ID */
    const std::string& id() const { return id_; }

    /** @brief 会话是否正在运行 */
    bool isRunning() const { return running_; }

    /**
     * @brief 注册数据回调
     * @param callback 终端输出数据回调
     *
     * 每次从 PTY 读取到数据时调用。
     */
    void onData(std::function<void(const std::string&)> callback);

    /**
     * @brief 注册退出回调
     * @param callback 进程退出回调（参数为退出码）
     */
    void onExit(std::function<void(int)> callback);

    /**
     * @brief 写入数据到终端
     * @param data 要写入的数据
     * @return 是否成功
     */
    bool write(const std::string& data);

    /**
     * @brief 调整终端大小
     * @param cols 新列数
     * @param rows 新行数
     * @return 是否成功
     */
    bool resize(uint32_t cols, uint32_t rows);

    /** @brief 关闭会话 */
    void close();

private:
    std::string id_;                                    ///< 会话ID
    bool running_ = false;                              ///< 运行状态
    std::unique_ptr<PtyHandle> pty_;                    ///< PTY 句柄
    std::function<void(const std::string&)> onData_;    ///< 数据回调
    std::function<void(int)> onExit_;                   ///< 退出回调
};

}  // namespace serctrl::shell
