/**
 * @file message.cpp
 * @brief 协议消息编解码实现
 *
 * 负责 Message 与 JSON 字节流之间的转换。
 * 使用 nlohmann/json 进行序列化/反序列化。
 *
 * 编码流程：
 * 1. 选择正确的载荷类型（编译时确定）
 * 2. 序列化为 JSON
 * 3. 转换为字节流
 *
 * 解码流程：
 * 1. 解析 JSON
 * 2. 根据消息类型反序列化载荷
 * 3. 返回类型安全的 Message
 */

#include "serctrl/protocol/codec.hpp"
#include "serctrl/protocol/message.hpp"

#include <nlohmann/json.hpp>
#include <cstring>

namespace serctrl::protocol {

using json = nlohmann::json;

// ═══════════════════════════════════════════
// JSON 序列化宏
// ═══════════════════════════════════════════
// 为每个载荷类型生成 to_json/from_json 函数

// 可选字段：使用默认值初始化，兼容旧对端发来的缺省字段（如握手随机数）
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AuthPayload, user, token, clientNonce)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AuthResultPayload, ok, error, serverNonce)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShellOpenPayload, requestId, cols, rows)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShellOpenResultPayload, requestId, sessionId, ok)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShellDataPayload, sessionId, data)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShellResizePayload, sessionId, cols, rows)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShellClosePayload, sessionId)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ExecPayload, requestId, command, timeout)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ExecResultPayload, requestId, exitCode, stdout, stderr)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileUploadPayload, requestId, remotePath, totalSize, done)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileUploadResultPayload, requestId, ok, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileDownloadPayload, requestId, remotePath)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileDownloadResultPayload, requestId, ok, totalSize, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileChunkPayload, requestId, index, total, data, done)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileDonePayload, requestId, sha256)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BatchUploadPayload, requestId, files, remoteDir)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BatchUploadResultPayload, requestId, ok, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BatchDownloadPayload, requestId, files)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BatchDownloadResultPayload, requestId, ok, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BatchChunkPayload, requestId, fileIndex, chunkIndex, totalChunks, data, done, filename)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BatchDonePayload, requestId, fileIndex, sha256)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ErrorPayload, message)

// ═══════════════════════════════════════════
// 空载荷的手动序列化
// ═══════════════════════════════════════════
// Ping、Pong、Close 没有字段，序列化为空 JSON 对象

void to_json(json& j, const PingPayload&) { j = json::object(); }
void from_json(const json&, PingPayload&) {}
void to_json(json& j, const PongPayload&) { j = json::object(); }
void from_json(const json&, PongPayload&) {}
void to_json(json& j, const ClosePayload&) { j = json::object(); }
void from_json(const json&, ClosePayload&) {}

namespace {

/**
 * @brief 载荷转 JSON
 * @param payload 载荷（std::variant）
 * @return JSON 对象
 *
 * 使用 std::visit 自动选择正确的载荷类型。
 */
json payloadToJson(const Payload& payload) {
    return std::visit([](const auto& p) -> json {
        return p;
    }, payload);
}

/**
 * @brief JSON 转载荷
 * @param type 消息类型
 * @param j JSON 对象
 * @return 类型安全的载荷
 *
 * 根据消息类型选择正确的反序列化函数。
 */
Payload jsonToPayload(MsgType type, const json& j) {
    switch (type) {
        case MsgType::Auth: return j.get<AuthPayload>();
        case MsgType::AuthResult: return j.get<AuthResultPayload>();
        case MsgType::ShellOpen: return j.get<ShellOpenPayload>();
        case MsgType::ShellOpenResult: return j.get<ShellOpenResultPayload>();
        case MsgType::ShellData: return j.get<ShellDataPayload>();
        case MsgType::ShellResize: return j.get<ShellResizePayload>();
        case MsgType::ShellClose: return j.get<ShellClosePayload>();
        case MsgType::Exec: return j.get<ExecPayload>();
        case MsgType::ExecResult: return j.get<ExecResultPayload>();
        case MsgType::FileUpload: return j.get<FileUploadPayload>();
        case MsgType::FileUploadResult: return j.get<FileUploadResultPayload>();
        case MsgType::FileDownload: return j.get<FileDownloadPayload>();
        case MsgType::FileDownloadResult: return j.get<FileDownloadResultPayload>();
        case MsgType::FileChunk: return j.get<FileChunkPayload>();
        case MsgType::FileDone: return j.get<FileDonePayload>();
        case MsgType::BatchUpload: return j.get<BatchUploadPayload>();
        case MsgType::BatchUploadResult: return j.get<BatchUploadResultPayload>();
        case MsgType::BatchDownload: return j.get<BatchDownloadPayload>();
        case MsgType::BatchDownloadResult: return j.get<BatchDownloadResultPayload>();
        case MsgType::BatchChunk: return j.get<BatchChunkPayload>();
        case MsgType::BatchDone: return j.get<BatchDonePayload>();
        case MsgType::Ping: return PingPayload{};
        case MsgType::Pong: return PongPayload{};
        case MsgType::Error: return j.get<ErrorPayload>();
        case MsgType::Close: return ClosePayload{};
        default: return ErrorPayload{"Unknown message type"};
    }
}

}  // namespace

std::vector<uint8_t> encode(const Message& msg) {
    // 构建 JSON 对象
    json j;
    j["type"] = static_cast<uint8_t>(msg.type);
    j["id"] = msg.id;
    j["ts"] = msg.ts;
    j["payload"] = payloadToJson(msg.payload);

    // 序列化为字节流
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::optional<Message> decode(std::span<const uint8_t> data) {
    try {
        // 解析 JSON
        std::string str(data.begin(), data.end());
        json j = json::parse(str);

        // 提取消息字段
        Message msg;
        msg.type = static_cast<MsgType>(j["type"].get<uint8_t>());
        msg.id = j["id"].get<uint64_t>();
        msg.ts = j["ts"].get<uint64_t>();
        msg.payload = jsonToPayload(msg.type, j["payload"]);

        return msg;
    } catch (...) {
        // 解析失败返回空
        return std::nullopt;
    }
}

std::vector<uint8_t> encodeFrame(uint8_t frameType, std::span<const uint8_t> nonce,
                                  std::span<const uint8_t> ciphertext) {
    // 帧格式：[1字节类型][nonce][密文]
    std::vector<uint8_t> frame;
    frame.reserve(1 + nonce.size() + ciphertext.size());
    frame.push_back(frameType);
    frame.insert(frame.end(), nonce.begin(), nonce.end());
    frame.insert(frame.end(), ciphertext.begin(), ciphertext.end());
    return frame;
}

std::optional<std::pair<uint8_t, std::vector<uint8_t>>> decodeFrame(std::span<const uint8_t> data) {
    // 至少需要1字节（帧类型）
    if (data.size() < 1) {
        return std::nullopt;
    }

    uint8_t frameType = data[0];
    // 载荷 = 帧类型之后的所有数据
    std::vector<uint8_t> payload(data.begin() + 1, data.end());

    return std::make_pair(frameType, std::move(payload));
}

}  // namespace serctrl::protocol
