/**
 * @file server.cpp
 * @brief WebSocket 服务端实现（uWebSockets）
 *
 * 使用 uWebSockets 提供 WebSocket 服务。
 *
 * 架构：
 * 1. 创建 uWS::App，注册 WebSocket 回调
 * 2. 新连接: 分配 ClientState，记录到 clients_ 映射表
 * 3. 收到消息: JSON 解码 → 检查认证 → 路由到 onClientMessage_
 * 4. 断开连接: 清理 ClientState，从 clients_ 移除
 * 5. 事件循环在独立线程运行
 *
 * 安全模型：
 * - 非认证消息（Auth/Ping 除外）会被拒绝
 * - 认证通过后设置 authenticated = true
 * - 每个客户端有独立的 ClientState
 */

#include "serctrl/transport/server.hpp"
#include "serctrl/protocol/codec.hpp"
#include "serctrl/crypto/encryptor.hpp"
#include "serctrl/util/logger.hpp"

#include <App.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace serctrl::transport {

/** @brief 每个 WebSocket 连接的数据 */
struct PerSocketData {
    std::string clientId;       ///< 客户端唯一ID
    bool authenticated = false; ///< 是否已认证
    std::string user;           ///< 认证用户名
};

/** @brief 服务器内部状态 */
struct Server::ServerState {
    std::unordered_map<std::string, uWS::WebSocket<false, true, PerSocketData>*> sockets; ///< clientId → ws 映射
    std::mutex mutex;                                                                      ///< 保护 sockets 映射
    std::atomic<bool> running{false};                                                      ///< 运行状态
    std::unique_ptr<std::thread> loopThread;                                               ///< 事件循环线程
    uint64_t nextClientId{1};                                                             ///< 自增客户端ID
};

Server::Server(const ServerConfig& config)
    : config_(config)
    , authenticator_(std::make_unique<auth::Authenticator>(config.authFile))
    , state_(std::make_unique<ServerState>()) {
}

Server::~Server() {
    stop();
}

void Server::setClientHandler(ClientHandler handler) {
    onClientMessage_ = std::move(handler);
}

void Server::start() {
    LOG_INFO("Server starting on port %d", config_.port);

    auto& s = *state_;
    s.running = true;

    // 启动事件循环线程
    s.loopThread = std::make_unique<std::thread>([this, &s]() {
        // 创建 uWS::App
        uWS::App()
            .ws<PerSocketData>("/*", {
                // 压缩选项
                .compression = uWS::SHARED_COMPRESSOR,
                .maxPayloadLength = 16 * 1024 * 1024,  // 16MB
                .idleTimeout = 120,                     // 120秒空闲超时
                .maxBackpressure = 1 * 1024 * 1024,    // 1MB
                .closeOnBackpressureLimit = false,
                .resetIdleTimeoutOnSend = true,
                .sendPingsAutomatically = true,

                // 连接建立
                .open = [this, &s](auto* ws) {
                    auto* data = ws->getUserData();
                    // 分配客户端ID
                    std::lock_guard<std::mutex> lock(s.mutex);
                    data->clientId = std::to_string(s.nextClientId++);
                    s.sockets[data->clientId] = ws;
                    LOG_INFO("Client %s connected (total: %zu)",
                             data->clientId.c_str(), s.sockets.size());
                },

                // 收到消息
                .message = [this, &s](auto* ws, std::string_view message, uWS::OpCode opCode) {
                    if (opCode != uWS::OpCode::TEXT) {
                        return;  // 仅处理文本消息
                    }

                    auto* data = ws->getUserData();

                    // 解码消息
                    auto msgData = std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(message.data()), message.size());
                    auto msg = protocol::decode(msgData);

                    if (!msg) {
                        LOG_WARN("Failed to decode message from %s", data->clientId.c_str());
                        return;
                    }

                    // 非认证消息需要先认证
                    if (!data->authenticated &&
                        msg->type != protocol::MsgType::Auth &&
                        msg->type != protocol::MsgType::Ping) {
                        // 发送认证失败响应
                        protocol::Message resp;
                        resp.type = protocol::MsgType::AuthResult;
                        resp.payload = protocol::AuthResultPayload{false, "Authentication required"};
                        auto encoded = protocol::encode(resp);
                        ws->send(std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()),
                                 uWS::OpCode::TEXT);
                        return;
                    }

                    // 处理认证消息
                    if (msg->type == protocol::MsgType::Auth) {
                        auto* authPayload = std::get_if<protocol::AuthPayload>(&msg->payload);
                        if (authPayload) {
                            bool ok = authenticator_->verify(authPayload->user, authPayload->token);
                            data->authenticated = ok;
                            data->user = authPayload->user;

                            protocol::Message resp;
                            resp.type = protocol::MsgType::AuthResult;
                            resp.payload = protocol::AuthResultPayload{ok, ok ? "" : "Invalid credentials"};
                            auto encoded = protocol::encode(resp);
                            ws->send(std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()),
                                     uWS::OpCode::TEXT);

                            LOG_INFO("Client %s: auth %s (user=%s)",
                                     data->clientId.c_str(),
                                     ok ? "ok" : "failed",
                                     authPayload->user.c_str());
                        }
                        return;
                    }

                    // 处理心跳
                    if (msg->type == protocol::MsgType::Ping) {
                        protocol::Message resp;
                        resp.type = protocol::MsgType::Pong;
                        resp.payload = protocol::PongPayload{};
                        auto encoded = protocol::encode(resp);
                        ws->send(std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()),
                                 uWS::OpCode::TEXT);
                        return;
                    }

                    // 路由到消息处理器
                    if (onClientMessage_) {
                        onClientMessage_(data->clientId, *msg);
                    }
                },

                // 连接关闭
                .close = [this, &s](auto* ws, int code, std::string_view reason) {
                    auto* data = ws->getUserData();
                    if (!data->clientId.empty()) {
                        LOG_INFO("Client %s disconnected (code=%d)",
                                 data->clientId.c_str(), code);
                        std::lock_guard<std::mutex> lock(s.mutex);
                        s.sockets.erase(data->clientId);
                    }
                },
            })
            .listen(config_.port, [this](auto* token) {
                if (token) {
                    listenSocket_ = token;
                    LOG_INFO("Server listening on port %d", config_.port);
                } else {
                    LOG_ERROR("Failed to listen on port %d", config_.port);
                    state_->running = false;
                }
            })
            .run();

        LOG_INFO("Server event loop stopped");
        s.running = false;
    });
}

void Server::stop() {
    auto& s = *state_;
    if (!s.running) {
        return;
    }

    LOG_INFO("Server stopping...");
    s.running = false;

    // uWS::App 的事件循环需要在 run() 被中断后退出
    // uWS 的 us_listen_socket_close 可以中断事件循环
    if (listenSocket_) {
        us_listen_socket_close(0, static_cast<struct us_listen_socket_t*>(listenSocket_));
        listenSocket_ = nullptr;
    }

    if (s.loopThread && s.loopThread->joinable()) {
        s.loopThread->join();
        s.loopThread.reset();
    }

    LOG_INFO("Server stopped");
}

bool Server::sendToClient(const std::string& clientId, const protocol::Message& msg) {
    auto& s = *state_;
    std::lock_guard<std::mutex> lock(s.mutex);

    auto it = s.sockets.find(clientId);
    if (it == s.sockets.end()) {
        return false;
    }

    auto encoded = protocol::encode(msg);
    auto status = it->second->send(
        std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()),
        uWS::OpCode::TEXT);

    return status == uWS::WebSocket<false, true, PerSocketData>::SendStatus::SUCCESS;
}

void Server::disconnectClient(const std::string& clientId) {
    auto& s = *state_;
    std::lock_guard<std::mutex> lock(s.mutex);

    auto it = s.sockets.find(clientId);
    if (it != s.sockets.end()) {
        it->second->close();
        s.sockets.erase(it);
    }
}

size_t Server::clientCount() const {
    auto& s = *state_;
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.sockets.size();
}

}  // namespace serctrl::transport
