/**
 * @file session.cpp
 * @brief 会话管理实现
 *
 * 管理单个远程终端会话的生命周期。
 * 每个会话关联一个 PTY 和一组回调函数。
 *
 * 使用流程：
 * 1. 创建 Session 对象
 * 2. 注册 onData/onExit 回调
 * 3. 调用 write() 发送数据
 * 4. 调用 resize() 调整终端大小
 * 5. 调用 close() 结束会话
 */

#include "serctrl/shell/session.hpp"

namespace serctrl::shell {

Session::Session(const std::string& id, const PtyOptions& opts)
    : id_(id)
    , pty_(std::make_unique<PtyHandle>(opts))
    , running_(pty_->isValid()) {
}

Session::~Session() {
    close();
}

void Session::onData(std::function<void(const std::string&)> callback) {
    onData_ = std::move(callback);
}

void Session::onExit(std::function<void(int)> callback) {
    onExit_ = std::move(callback);
}

bool Session::write(const std::string& data) {
    if (!running_ || !pty_) {
        return false;
    }
    // 将字符串数据写入 PTY
    return pty_->writeData(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size())) > 0;
}

bool Session::resize(uint32_t cols, uint32_t rows) {
    if (!running_ || !pty_) {
        return false;
    }
    return pty_->resize(cols, rows);
}

ssize_t Session::readPTY(char* buf, size_t len) {
    if (!running_ || !pty_) {
        return -1;
    }
    return pty_->readData(std::span<uint8_t>(reinterpret_cast<uint8_t*>(buf), len));
}

void Session::close() {
    running_ = false;
    if (pty_) {
        pty_->closeFd();
    }
}

}  // namespace serctrl::shell
