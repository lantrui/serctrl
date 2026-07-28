#pragma once

/**
 * @file connection.hpp
 * @brief 连接抽象层
 *
 * 定义 WebSocket 连接的抽象接口。
 * Server 和 Client 都继承此基类。
 *
 * 职责：
 * - 消息收发
 * - 加密/解密
 * - 错误处理
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "serctrl/crypto/encryptor.hpp"
#include "serctrl/protocol/message.hpp"

namespace serctrl::transport {

/** @brief 消息接收回调 */
using MessageHandler = std::function<void(const protocol::Message&)>;
/** @brief 错误回调 */
using ErrorHandler = std::function<void(const std::string&)>;

/**
 * @brief 连接抽象基类
 *
 * 封装 WebSocket 连接的通用逻辑。
 * 子类实现具体的传输方式（uWebSockets 等）。
 *
 * 线程安全：所有方法应在同一事件循环线程调用。
 */
class Connection {
public:
    Connection();
    virtual ~Connection();

    /**
     * @brief 注册消息回调
     * @param handler 收到消息时调用
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief 注册错误回调
     * @param handler 发生错误时调用
     */
    void setErrorHandler(ErrorHandler handler);

    /**
     * @brief 发送消息
     * @param msg 要发送的消息
     * @return 是否成功
     */
    virtual bool send(const protocol::Message& msg) = 0;

    /** @brief 是否已连接 */
    virtual bool isConnected() const = 0;

    /** @brief 关闭连接 */
    virtual void close() = 0;

protected:
    /**
     * @brief 处理接收到的原始数据
     * @param data 原始字节流
     *
     * 解密 → 解码 → 调用 onMessage_ 回调
     */
    void handleMessage(std::span<const uint8_t> data);

    /**
     * @brief 处理错误
     * @param error 错误描述
     */
    void handleError(const std::string& error);

    std::unique_ptr<crypto::Encryptor> encryptor_;  ///< 加密器（已连接时有效）
    MessageHandler onMessage_;                       ///< 消息回调
    ErrorHandler onError_;                           ///< 错误回调
};

}  // namespace serctrl::transport
