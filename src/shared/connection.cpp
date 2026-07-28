/**
 * @file connection.cpp
 * @brief 传输连接基类实现
 *
 * 提供消息处理和错误处理的基础设施。
 * 具体的 WebSocket 实现在 server.cpp 和 client.cpp 中。
 */

#include "serctrl/transport/connection.hpp"
#include "serctrl/protocol/codec.hpp"

namespace serctrl::transport {

Connection::Connection() = default;
Connection::~Connection() = default;

void Connection::setMessageHandler(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void Connection::setErrorHandler(ErrorHandler handler) {
    onError_ = std::move(handler);
}

void Connection::handleMessage(std::span<const uint8_t> data) {
    if (!onMessage_) {
        return;
    }

    // 解码消息
    auto msg = protocol::decode(data);
    if (msg) {
        // 如果是加密消息，需要先解密
        if (encryptor_ && data.size() > 0 && data[0] == protocol::FRAME_ENCRYPTED) {
            // TODO: 实现加密消息解密
        }
        onMessage_(*msg);
    }
}

void Connection::handleError(const std::string& error) {
    if (onError_) {
        onError_(error);
    }
}

}  // namespace serctrl::transport
