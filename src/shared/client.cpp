/**
 * @file client.cpp
 * @brief WebSocket 客户端实现（IXWebSocket）
 *
 * 使用 IXWebSocket 连接到 SerCTRL 服务器。
 *
 * 连接流程：
 * 1. connect() 建立 WebSocket 连接
 * 2. 连接成功后调用 onConnect_ 回调
 * 3. 自动发送认证消息
 * 4. 收到消息时解码并调用 onMessage_ 回调
 * 5. 断开后自动重连（指数退避）
 *
 * 重连策略：
 * - 初始延迟 1000ms，每次翻倍
 * - 最大延迟 30000ms
 * - 连接成功后重置延迟
 */

#include "serctrl/transport/client.hpp"
#include "serctrl/protocol/codec.hpp"
#include "serctrl/util/logger.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <chrono>
#include <thread>

namespace serctrl::transport {

/** @brief 客户端内部状态（PIMPL） */
struct Client::ClientState {
    std::unique_ptr<ix::WebSocket> ws;              ///< IXWebSocket 实例
    std::unique_ptr<std::thread> reconnectThread;   ///< 重连线程
    std::atomic<bool> shouldReconnect{false};       ///< 是否应重连
    std::atomic<bool> stopping{false};              ///< 是否正在停止
    uint32_t currentDelay = 0;                      ///< 当前重连延迟
};

Client::Client()
    : state_(std::make_unique<ClientState>()) {
}

Client::~Client() {
    disconnect();
}

bool Client::connect(const ClientConfig& config) {
    config_ = config;
    auto& s = *state_;

    s.stopping = false;
    s.shouldReconnect = true;

    // 创建 WebSocket 客户端
    s.ws = std::make_unique<ix::WebSocket>();
    s.ws->setUrl("ws://" + config.host + ":" + std::to_string(config.port));

    // 收到消息回调
    s.ws->setOnMessageCallback([this, &s](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                LOG_INFO("Connected to %s:%d", config_.host.c_str(), config_.port);
                connected_ = true;
                state_->currentDelay = config_.reconnectDelay; // 重置重连延迟
                if (onConnect_) {
                    onConnect_();
                }
                // 自动发送认证
                if (!config_.user.empty() && !config_.token.empty()) {
                    authenticate(config_.user, config_.token);
                }
                break;

            case ix::WebSocketMessageType::Message: {
                auto data = std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(msg->str.data()), msg->str.size());
                handleMessage(data);
                break;
            }

            case ix::WebSocketMessageType::Close:
                LOG_INFO("Disconnected from %s:%d (code=%d, reason=%s)",
                         config_.host.c_str(), config_.port,
                         msg->closeInfo.code, msg->closeInfo.reason.c_str());
                connected_ = false;
                if (onDisconnect_) {
                    onDisconnect_();
                }
                // 触发自动重连
                if (s.shouldReconnect && !s.stopping) {
                    startReconnect();
                }
                break;

            case ix::WebSocketMessageType::Error:
                LOG_ERROR("Connection error: %s", msg->errorInfo.reason.c_str());
                handleError(msg->errorInfo.reason);
                break;

            default:
                break;
        }
    });

    LOG_INFO("Connecting to %s:%d", config_.host.c_str(), config_.port);
    s.ws->start();

    return true;
}

bool Client::reconnect() {
    LOG_INFO("Reconnecting...");
    return connect(config_);
}

void Client::disconnect() {
    auto& s = *state_;
    s.stopping = true;
    s.shouldReconnect = false;

    // 停止重连线程
    if (s.reconnectThread && s.reconnectThread->joinable()) {
        s.reconnectThread->join();
        s.reconnectThread.reset();
    }

    // 断开 WebSocket
    if (s.ws) {
        s.ws->stop();
        s.ws.reset();
    }

    connected_ = false;
}

bool Client::send(const protocol::Message& msg) {
    auto& s = *state_;
    if (!connected_ || !s.ws) {
        return false;
    }

    auto encoded = protocol::encode(msg);
    auto result = s.ws->sendText(
        std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));

    return result.success;
}

bool Client::isConnected() const {
    return connected_;
}

void Client::close() {
    disconnect();
}

bool Client::authenticate(const std::string& user, const std::string& token) {
    protocol::Message msg;
    msg.type = protocol::MsgType::Auth;
    msg.id = 1;
    msg.ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    msg.payload = protocol::AuthPayload{user, token};

    return send(msg);
}

void Client::onConnect(std::function<void()> handler) {
    onConnect_ = std::move(handler);
}

void Client::onDisconnect(std::function<void()> handler) {
    onDisconnect_ = std::move(handler);
}

void Client::startReconnect() {
    auto& s = *state_;
    if (s.reconnectThread && s.reconnectThread->joinable()) {
        return;  // 重连线程已在运行
    }

    s.reconnectThread = std::make_unique<std::thread>([this, &s]() {
        uint32_t delay = s.currentDelay;

        while (s.shouldReconnect && !s.stopping) {
            LOG_INFO("Reconnecting in %dms...", delay);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            if (!s.shouldReconnect || s.stopping) {
                break;
            }

            // 尝试重连
            s.ws->stop();
            s.ws->start();

            // 指数退避
            delay = std::min(delay * 2, config_.maxReconnectDelay);

            // 等一下看是否连上了
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (connected_) {
                break;  // 连接成功，退出重连循环
            }
        }
    });
}

}  // namespace serctrl::transport
