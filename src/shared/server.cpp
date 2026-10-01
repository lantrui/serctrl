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
#include <sodium.h>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <vector>

namespace serctrl::transport {

/** @brief 每个 WebSocket 连接的数据 */
struct PerSocketData {
    std::string clientId;                                   ///< 客户端唯一ID
    bool authenticated = false;                             ///< 是否已认证
    std::string user;                                       ///< 认证用户名
    std::unique_ptr<crypto::Encryptor> encryptor;            ///< 加密器（认证后启用）
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

                    std::lock_guard<std::mutex> lock(s.mutex);

                    // 强制执行最大连接数限制，防止资源耗尽（DoS）。
                    // 历史实现从未检查该限制，攻击者可耗尽服务器资源。
                    if (s.sockets.size() >= config_.maxConnections) {
                        LOG_WARN("Max connections (%u) reached, rejecting new client",
                                 config_.maxConnections);
                        // uWS 的 end() 仅支持 (code, message)，无 payload 重载。
                        ws->end(1013, "Too many connections");
                        return;
                    }

                    // 分配客户端ID
                    data->clientId = std::to_string(s.nextClientId++);
                    s.sockets[data->clientId] = ws;
                    LOG_INFO("Client %s connected (total: %zu)",
                             data->clientId.c_str(), s.sockets.size());
                },

                // 收到消息
                .message = [this, &s](auto* ws, std::string_view message, uWS::OpCode opCode) {
                    if (opCode != uWS::OpCode::TEXT && opCode != uWS::OpCode::BINARY) {
                        return;
                    }

                    auto* data = ws->getUserData();
                    auto raw = std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(message.data()), message.size());

                    // 如果是二进制消息 + 认证后 = 加密消息，先解密
                    std::vector<uint8_t> decryptedBuf;
                    if (opCode == uWS::OpCode::BINARY && data->encryptor) {
                        try {
                            decryptedBuf = data->encryptor->decrypt(raw);
                            raw = decryptedBuf;
                        } catch (...) {
                            LOG_WARN("Decryption failed from %s", data->clientId.c_str());
                            return;
                        }
                    }

                    auto msg = protocol::decode(raw);
                    if (!msg) {
                        LOG_WARN("Failed to decode message from %s", data->clientId.c_str());
                        return;
                    }

                    // 非认证消息需要先认证
                    if (!data->authenticated &&
                        msg->type != protocol::MsgType::Auth &&
                        msg->type != protocol::MsgType::Ping) {
                        protocol::Message resp;
                        resp.type = protocol::MsgType::AuthResult;
                        resp.payload = protocol::AuthResultPayload{false, "Authentication required", ""};
                        auto encoded = protocol::encode(resp);
                        ws->send(std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()),
                                 uWS::OpCode::TEXT);
                        return;
                    }

                    // 【降级防护】加密通道建立后拒绝一切明文（TEXT）帧：
                    // 历史实现对已加密连接仍接受明文消息，攻击者（如位于代理处）
                    // 可诱导或伪造明文帧绕过加密层。此后仅接受 BINARY 密文，
                    // 心跳保活由 uWS 自动 ping 与已加密的 Ping/Pong 承担。
                    if (data->encryptor && opCode == uWS::OpCode::TEXT) {
                        LOG_WARN("Rejected plaintext frame from encrypted client %s (possible downgrade)",
                                 data->clientId.c_str());
                        return;
                    }

                    // 处理认证消息
                    if (msg->type == protocol::MsgType::Auth) {
                        auto* authPayload = std::get_if<protocol::AuthPayload>(&msg->payload);
                        if (authPayload) {
                            bool ok = authenticator_->verify(authPayload->user, authPayload->token);
                            data->authenticated = ok;
                            data->user = authPayload->user;

                            // 认证成功后派生会话密钥：
                            // 混入服务端一次性随机数（以及客户端随机数，若提供），
                            // 使每个连接的会话密钥唯一——取代历史的静态密钥
                            // （同一令牌的每次连接密钥相同，无法防重放/单会话泄露扩散）。
                            std::string serverNonceHex;
                            if (ok) {
                                auth::HandshakeNonce serverNonce = auth::Authenticator::generateHandshakeNonce();
                                serverNonceHex = auth::Authenticator::nonceToHex(serverNonce);

                                std::vector<uint8_t> secret;
                                secret.reserve(authPayload->token.size() + auth::HANDSHAKE_NONCE_BYTES * 2);
                                secret.insert(secret.end(), authPayload->token.begin(), authPayload->token.end());
                                secret.insert(secret.end(), serverNonce.begin(), serverNonce.end());
                                if (authPayload->clientNonce.size() == auth::HANDSHAKE_NONCE_BYTES * 2) {
                                    auto cn = auth::Authenticator::hexToNonce(authPayload->clientNonce);
                                    secret.insert(secret.end(), cn.begin(), cn.end());
                                }
                                crypto::Key sessionKey = auth::Authenticator::deriveSessionKey(secret);
                                sodium_memzero(secret.data(), secret.size());  // 及时清零密钥材料
                                data->encryptor = std::make_unique<crypto::Encryptor>(sessionKey);
                            }

                            protocol::Message resp;
                            resp.type = protocol::MsgType::AuthResult;
                            resp.payload = protocol::AuthResultPayload{
                                ok, ok ? "" : "Invalid credentials", serverNonceHex};
                            auto encoded = protocol::encode(resp);
                            ws->send(std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size()),
                                     uWS::OpCode::TEXT);

                            LOG_INFO("Client %s: auth %s (user=%s%s)", data->clientId.c_str(),
                                     ok ? "ok" : "failed", authPayload->user.c_str(),
                                     ok ? ", encrypted" : "");
                        }
                        return;
                    }

                    // 处理心跳（加密后仍允许明文 ping）
                    if (msg->type == protocol::MsgType::Ping) {
                        protocol::Message resp;
                        resp.type = protocol::MsgType::Pong;
                        resp.payload = protocol::PongPayload{};
                        auto encoded = protocol::encode(resp);
                        auto sv = std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size());
                        if (data->encryptor) {
                            auto ct = data->encryptor->encrypt(encoded);
                            ws->send(std::string_view(reinterpret_cast<const char*>(ct.data()), ct.size()),
                                     uWS::OpCode::BINARY);
                        } else {
                            ws->send(sv, uWS::OpCode::TEXT);
                        }
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
    auto* data = it->second->getUserData();

    uWS::OpCode op = uWS::OpCode::TEXT;
    std::vector<uint8_t> sendBuf;

    // 如果客户端已加密，用二进制帧发送密文
    if (data->encryptor) {
        sendBuf = data->encryptor->encrypt(encoded);
        op = uWS::OpCode::BINARY;
        encoded = sendBuf;
    }

    auto sv = std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    auto status = it->second->send(sv, op);

    return status == uWS::WebSocket<false, true, PerSocketData>::SendStatus::SUCCESS;
}

void Server::disconnectClient(const std::string& clientId) {
    auto& s = *state_;
    std::lock_guard<std::mutex> lock(s.mutex);

    auto it = s.sockets.find(clientId);
    if (it != s.sockets.end()) {
        // 用 end() 发送正常的 WebSocket Close 帧（close() 是裸 TCP 关闭，
        // 会让对端看到协议错误而非干净断开）；close 回调负责从映射表移除。
        it->second->end(1000, "");
    }
}

size_t Server::clientCount() const {
    auto& s = *state_;
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.sockets.size();
}

}  // namespace serctrl::transport
