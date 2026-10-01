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
#include "serctrl/crypto/encryptor.hpp"
#include "serctrl/auth/authenticator.hpp"
#include "serctrl/util/logger.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <sodium.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

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
                state_->currentDelay = config_.reconnectDelay;
                if (onConnect_) {
                    onConnect_();
                }
                if (!config_.user.empty() && !config_.token.empty()) {
                    authenticate(config_.user, config_.token);
                }
                break;

            case ix::WebSocketMessageType::Message: {
                auto raw = std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(msg->str.data()), msg->str.size());

                // 加密消息：先解密再解码
                if (encryptor_ && msg->binary) {
                    try {
                        auto plaintext = encryptor_->decrypt(raw);
                        handleMessage(plaintext);
                    } catch (...) {
                        LOG_ERROR("Decryption failed");
                    }
                } else if (encryptor_ && !msg->binary) {
                    // 【降级防护】加密通道建立后拒绝明文帧（与服务端策略对称）
                    LOG_WARN("Rejected plaintext frame on encrypted channel (possible downgrade)");
                } else {
                    handleMessage(raw);
                }
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

    if (encryptor_) {
        auto ciphertext = encryptor_->encrypt(encoded);
        return s.ws->sendBinary(
            std::string(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size())
        ).success;
    }

    return s.ws->sendText(
        std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size())
    ).success;
}

bool Client::isConnected() const {
    return connected_;
}

void Client::close() {
    disconnect();
}

bool Client::authenticate(const std::string& user, const std::string& token) {
    // 每次连接（含自动重连）生成全新的一次性握手随机数，
    // 保证派生出的会话密钥每连接唯一。
    clientNonceHex_ = auth::Authenticator::nonceToHex(
        auth::Authenticator::generateHandshakeNonce());

    protocol::Message msg;
    msg.type = protocol::MsgType::Auth;
    msg.id = 1;
    msg.ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    msg.payload = protocol::AuthPayload{user, token, clientNonceHex_};

    return send(msg);
}

void Client::handleAuthResult(const protocol::AuthResultPayload& result) {
    if (!result.ok) {
        handleError(result.error.empty() ? "Authentication failed" : result.error);
        return;
    }
    if (result.serverNonce.size() != auth::HANDSHAKE_NONCE_BYTES * 2) {
        // 服务端未提供握手随机数：不能与服务端一致地派生会话密钥。
        // 绝不回退到旧的静态密钥（可被重放），保持明文并显式报错。
        encryptor_.reset();
        LOG_ERROR("Auth ok but serverNonce missing/invalid; encryption unavailable");
        handleError("Server did not provide handshake nonce; encrypted channel unavailable");
        return;
    }

    // 与服务端完全一致的派生公式：token || serverNonce || clientNonce
    std::vector<uint8_t> secret;
    secret.reserve(config_.token.size() + auth::HANDSHAKE_NONCE_BYTES * 2);
    secret.insert(secret.end(), config_.token.begin(), config_.token.end());
    auto sn = auth::Authenticator::hexToNonce(result.serverNonce);
    secret.insert(secret.end(), sn.begin(), sn.end());
    if (clientNonceHex_.size() == auth::HANDSHAKE_NONCE_BYTES * 2) {
        auto cn = auth::Authenticator::hexToNonce(clientNonceHex_);
        secret.insert(secret.end(), cn.begin(), cn.end());
    }
    crypto::Key sessionKey = auth::Authenticator::deriveSessionKey(secret);
    sodium_memzero(secret.data(), secret.size());  // 及时清零密钥材料
    encryptor_ = std::make_unique<crypto::Encryptor>(sessionKey);
    LOG_INFO("Encryption enabled (per-session key derived from handshake nonces)");
}

void Client::enableEncryption(const std::string& token) {
    // 【兼容旧协议】用令牌指纹派生静态会话密钥。
    // 该方式同一令牌的每次连接密钥相同，存在重放风险；仅当对端为
    // 不理解握手随机数的旧版服务端时使用。新代码请调用 handleAuthResult()。
    auto fp = auth::Authenticator::computeFingerprint(
        auth::Authenticator::hexToToken(token));
    serctrl::crypto::Key sessionKey;
    std::copy(fp.begin(), fp.end(), sessionKey.begin());
    encryptor_ = std::make_unique<serctrl::crypto::Encryptor>(sessionKey);
    LOG_WARN("Encryption enabled with LEGACY static key (no forward isolation)");
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
