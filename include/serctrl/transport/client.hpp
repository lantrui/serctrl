#pragma once

/**
 * @file client.hpp
 * @brief WebSocket 客户端
 *
 * 基于 uWebSockets 的 WebSocket 客户端。
 * 支持自动重连（指数退避）和心跳检测。
 *
 * 连接流程：
 * 1. connect() 建立 WebSocket 连接
 * 2. authenticate() 发送认证
 * 3. 认证成功后可收发消息
 * 4. 断开后自动重连
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "serctrl/protocol/message.hpp"
#include "serctrl/transport/connection.hpp"

namespace serctrl::transport {

/**
 * @brief 客户端配置
 *
 * 连接服务器时的参数。
 */
struct ClientConfig {
    std::string host;                       ///< 服务器地址
    uint16_t port = 8080;                   ///< 服务器端口
    std::string user;                       ///< 用户名
    std::string token;                      ///< 认证令牌
    uint32_t reconnectDelay = 1000;         ///< 初始重连延迟（毫秒）
    uint32_t maxReconnectDelay = 30000;     ///< 最大重连延迟（毫秒）
    uint32_t heartbeatInterval = 5000;      ///< 心跳间隔（毫秒）
};

/**
 * @brief WebSocket 客户端
 *
 * 继承 Connection，实现 WebSocket 客户端逻辑。
 * 支持自动重连和心跳检测。
 *
 * 用法：
 * @code
 *   Client client;
 *   client.onConnect([]() { LOG_INFO("Connected"); });
 *   client.onDisconnect([]() { LOG_INFO("Disconnected"); });
 *   client.setMessageHandler([](const Message& msg) {
 *       // 处理消息
 *   });
 *   client.connect({"192.168.1.100", 8080, "admin", "token"});
 * @endcode
 */
class Client : public Connection {
public:
    Client();
    ~Client();

    /**
     * @brief 连接到服务器
     * @param config 连接配置
     * @return 是否成功发起连接
     */
    bool connect(const ClientConfig& config);

    /**
     * @brief 重新连接
     * @return 是否成功发起重连
     */
    bool reconnect();

    /** @brief 断开连接 */
    void disconnect();

    /** @brief 发送消息 */
    bool send(const protocol::Message& msg) override;

    /** @brief 是否已连接 */
    bool isConnected() const override;

    /** @brief 关闭连接 */
    void close() override;

    /**
     * @brief 发送认证
     * @param user 用户名
     * @param token 认证令牌
     * @return 是否成功发送
     */
    bool authenticate(const std::string& user, const std::string& token);

    /**
     * @brief 注册连接成功回调
     * @param handler 连接建立时调用
     */
    void onConnect(std::function<void()> handler);

    /**
     * @brief 注册断开连接回调
     * @param handler 连接断开时调用
     */
    void onDisconnect(std::function<void()> handler);

private:
    ClientConfig config_;               ///< 连接配置
    bool connected_ = false;            ///< 连接状态
    std::function<void()> onConnect_;   ///< 连接成功回调
    std::function<void()> onDisconnect_; ///< 断开连接回调

    struct ClientState;
    std::unique_ptr<ClientState> state_; ///< 内部状态（PIMPL）

    /** @brief 启动自动重连 */
    void startReconnect();
};

}  // namespace serctrl::transport
