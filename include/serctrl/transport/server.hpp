#pragma once

/**
 * @file server.hpp
 * @brief WebSocket 服务端
 *
 * 基于 uWebSockets 的 WebSocket 服务器。
 * 管理多个客户端连接，处理认证和消息路由。
 *
 * 职责：
 * - 监听端口，接受连接
 * - 认证客户端
 * - 路由消息到业务逻辑
 * - 管理连接生命周期
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "serctrl/auth/authenticator.hpp"
#include "serctrl/config/config.hpp"
#include "serctrl/protocol/message.hpp"
#include "serctrl/transport/connection.hpp"

namespace serctrl::transport {

/**
 * @brief 服务器配置
 *
 * 启动服务器时的参数。
 */
struct ServerConfig {
    uint16_t port = 8080;               ///< 监听端口
    uint32_t maxConnections = 100;      ///< 最大连接数
    std::string rootDir;                ///< 沙盒根目录（为空则不限制）
    std::string authFile;               ///< 认证文件路径
};

/**
 * @brief 客户端消息回调
 *
 * 参数：(客户端ID, 消息)
 */
using ClientHandler = std::function<void(const std::string& clientId, const protocol::Message& msg)>;

/**
 * @brief WebSocket 服务器
 *
 * 管理所有客户端连接和消息路由。
 *
 * 用法：
 * @code
 *   ServerConfig config;
 *   config.port = 8080;
 *   Server server(config);
 *   server.setClientHandler([](const std::string& id, const Message& msg) {
 *       // 处理客户端消息
 *   });
 *   server.start();
 * @endcode
 */
class Server {
public:
    /**
     * @brief 构造服务器
     * @param config 服务器配置
     */
    Server(const ServerConfig& config);
    ~Server();

    /**
     * @brief 注册客户端消息处理器
     * @param handler 消息回调
     */
    void setClientHandler(ClientHandler handler);

    /** @brief 启动服务器（非阻塞） */
    void start();

    /** @brief 停止服务器 */
    void stop();

    /**
     * @brief 向客户端发送消息
     * @param clientId 客户端ID
     * @param msg 要发送的消息
     * @return 是否成功
     */
    bool sendToClient(const std::string& clientId, const protocol::Message& msg);

    /**
     * @brief 断开指定客户端
     * @param clientId 客户端ID
     */
    void disconnectClient(const std::string& clientId);

    /** @brief 当前连接的客户端数量 */
    size_t clientCount() const;

private:
    ServerConfig config_;                                  ///< 服务器配置
    std::unique_ptr<auth::Authenticator> authenticator_;   ///< 认证器
    ClientHandler onClientMessage_;                        ///< 消息回调
    void* listenSocket_ = nullptr;                         ///< 监听 socket（类型擦除）
    struct ServerState;
    std::unique_ptr<ServerState> state_;                   ///< 内部状态（PIMPL，避免头文件暴露 uWS）
};

}  // namespace serctrl::transport
