#pragma once

/**
 * @file message.hpp
 * @brief 协议消息类型定义
 *
 * 定义 SerCTRL 通信协议的所有消息类型。
 * 使用 std::variant 实现类型安全的消息载荷。
 *
 * 协议结构：
 *   Message = { type, id, ts, payload }
 *   - type: 消息类型（枚举）
 *   - id: 请求ID（用于匹配请求和响应）
 *   - ts: 时间戳（毫秒）
 *   - payload: 类型安全的载荷（std::variant）
 */

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace serctrl::protocol {

/**
 * @brief 消息类型枚举
 *
 * 按功能分组：
 * - 0x01-0x02: 认证
 * - 0x10-0x14: Shell 会话
 * - 0x20-0x21: 命令执行
 * - 0x30-0x35: 单文件传输
 * - 0x40-0x45: 批量传输
 * - 0xF0-0xFF: 控制消息
 */
enum class MsgType : uint8_t {
    // ── 认证 ──────────────────────────────
    Auth = 0x01,           ///< 客户端发送认证请求
    AuthResult = 0x02,     ///< 服务端返回认证结果

    // ── Shell 会话 ────────────────────────
    ShellOpen = 0x10,      ///< 请求打开 Shell
    ShellOpenResult = 0x11,///< Shell 打开结果
    ShellData = 0x12,      ///< Shell 数据（双向）
    ShellResize = 0x13,    ///< 调整终端大小
    ShellClose = 0x14,     ///< 关闭 Shell

    // ── 命令执行 ──────────────────────────
    Exec = 0x20,           ///< 执行远程命令
    ExecResult = 0x21,     ///< 命令执行结果

    // ── 单文件传输 ────────────────────────
    FileUpload = 0x30,     ///< 上传文件请求
    FileUploadResult = 0x31,///< 上传结果
    FileDownload = 0x32,   ///< 下载文件请求
    FileDownloadResult = 0x33,///< 下载结果
    FileChunk = 0x34,      ///< 文件数据块
    FileDone = 0x35,       ///< 文件传输完成

    // ── 批量传输 ──────────────────────────
    BatchUpload = 0x40,    ///< 批量上传请求
    BatchUploadResult = 0x41,///< 批量上传结果
    BatchDownload = 0x42,  ///< 批量下载请求
    BatchDownloadResult = 0x43,///< 批量下载结果
    BatchChunk = 0x44,     ///< 批量数据块
    BatchDone = 0x45,      ///< 批量传输完成

    // ── 控制消息 ──────────────────────────
    Ping = 0xF0,           ///< 心跳探测
    Pong = 0xF1,           ///< 心跳响应
    Error = 0xFE,          ///< 错误消息
    Close = 0xFF,          ///< 关闭连接
};

// ═══════════════════════════════════════════
// 认证载荷
// ═══════════════════════════════════════════

/** @brief 认证请求载荷 */
struct AuthPayload {
    std::string user;   ///< 用户名
    std::string token;  ///< 认证令牌
};

/** @brief 认证结果载荷 */
struct AuthResultPayload {
    bool ok;                ///< 是否成功
    std::string error;      ///< 失败原因（仅失败时有值）
};

// ═══════════════════════════════════════════
// Shell 会话载荷
// ═══════════════════════════════════════════

/** @brief Shell 打开请求 */
struct ShellOpenPayload {
    std::string requestId;  ///< 请求ID（用于匹配响应）
    uint32_t cols = 80;     ///< 终端列数
    uint32_t rows = 24;     ///< 终端行数
};

/** @brief Shell 打开结果 */
struct ShellOpenResultPayload {
    std::string requestId;  ///< 对应的请求ID
    std::string sessionId;  ///< 会话ID（后续操作用）
    bool ok;                ///< 是否成功
};

/** @brief Shell 数据（双向传输） */
struct ShellDataPayload {
    std::string sessionId;  ///< 会话ID
    std::string data;       ///< 终端数据（输入/输出）
};

/** @brief 调整终端大小 */
struct ShellResizePayload {
    std::string sessionId;  ///< 会话ID
    uint32_t cols;          ///< 新列数
    uint32_t rows;          ///< 新行数
};

/** @brief 关闭 Shell */
struct ShellClosePayload {
    std::string sessionId;  ///< 会话ID
};

// ═══════════════════════════════════════════
// 命令执行载荷
// ═══════════════════════════════════════════

/** @brief 执行远程命令请求 */
struct ExecPayload {
    std::string requestId;  ///< 请求ID
    std::string command;    ///< 要执行的命令
    uint32_t timeout = 0;   ///< 超时时间（毫秒，0=不限）
};

/** @brief 命令执行结果 */
struct ExecResultPayload {
    std::string requestId;  ///< 对应的请求ID
    int exitCode = 0;       ///< 退出码
    std::string stdout;     ///< 标准输出
    std::string stderr;     ///< 标准错误
};

// ═══════════════════════════════════════════
// 单文件传输载荷
// ═══════════════════════════════════════════

/** @brief 文件上传请求 */
struct FileUploadPayload {
    std::string requestId;      ///< 请求ID
    std::string remotePath;     ///< 远端路径
    uint64_t totalSize = 0;     ///< 文件总大小
    bool done = false;          ///< 是否为最后一个块
};

/** @brief 文件上传结果 */
struct FileUploadResultPayload {
    std::string requestId;  ///< 对应的请求ID
    bool ok;                ///< 是否成功
    std::string error;      ///< 失败原因
};

/** @brief 文件下载请求 */
struct FileDownloadPayload {
    std::string requestId;  ///< 请求ID
    std::string remotePath; ///< 远端路径
};

/** @brief 文件下载结果 */
struct FileDownloadResultPayload {
    std::string requestId;  ///< 对应的请求ID
    bool ok;                ///< 是否成功
    uint64_t totalSize = 0; ///< 文件总大小
    std::string error;      ///< 失败原因
};

/** @brief 文件数据块 */
struct FileChunkPayload {
    std::string requestId;          ///< 对应的请求ID
    uint32_t index = 0;             ///< 块序号
    uint32_t total = 0;             ///< 总块数
    std::vector<uint8_t> data;      ///< 块数据
    bool done = false;              ///< 是否为最后一块
};

/** @brief 文件传输完成（含校验） */
struct FileDonePayload {
    std::string requestId;  ///< 对应的请求ID
    std::string sha256;     ///< 文件 SHA-256 校验和
};

// ═══════════════════════════════════════════
// 批量传输载荷
// ═══════════════════════════════════════════

/** @brief 批量上传请求 */
struct BatchUploadPayload {
    std::string requestId;              ///< 请求ID
    std::vector<std::string> files;     ///< 待上传文件列表
    std::string remoteDir;              ///< 远端目标目录
};

/** @brief 批量上传结果 */
struct BatchUploadResultPayload {
    std::string requestId;  ///< 对应的请求ID
    bool ok;                ///< 是否成功
    std::string error;      ///< 失败原因
};

/** @brief 批量下载请求 */
struct BatchDownloadPayload {
    std::string requestId;              ///< 请求ID
    std::vector<std::string> files;     ///< 待下载文件列表
};

/** @brief 批量下载结果 */
struct BatchDownloadResultPayload {
    std::string requestId;  ///< 对应的请求ID
    bool ok;                ///< 是否成功
    std::string error;      ///< 失败原因
};

/** @brief 批量数据块 */
struct BatchChunkPayload {
    std::string requestId;          ///< 对应的请求ID
    uint32_t fileIndex = 0;         ///< 文件序号
    uint32_t chunkIndex = 0;        ///< 块序号
    uint32_t totalChunks = 0;       ///< 该文件的总块数
    std::vector<uint8_t> data;      ///< 块数据
    bool done = false;              ///< 是否为该文件最后一块
    std::string filename;           ///< 文件名
};

/** @brief 单个文件批量传输完成 */
struct BatchDonePayload {
    std::string requestId;      ///< 对应的请求ID
    uint32_t fileIndex = 0;     ///< 文件序号
    std::string sha256;         ///< 该文件的 SHA-256 校验和
};

// ═══════════════════════════════════════════
// 控制消息载荷
// ═══════════════════════════════════════════

/** @brief 心跳探测（空载荷） */
struct PingPayload {};

/** @brief 心跳响应（空载荷） */
struct PongPayload {};

/** @brief 错误消息 */
struct ErrorPayload {
    std::string message;  ///< 错误描述
};

/** @brief 关闭连接（空载荷） */
struct ClosePayload {};

// ═══════════════════════════════════════════
// 组合类型
// ═══════════════════════════════════════════

/**
 * @brief 载荷类型（std::variant）
 *
 * 所有可能的载荷类型的联合体，使用 std::visit 访问。
 * 编译时保证类型安全，避免 void* 或 any 类型。
 */
using Payload = std::variant<
    AuthPayload,
    AuthResultPayload,
    ShellOpenPayload,
    ShellOpenResultPayload,
    ShellDataPayload,
    ShellResizePayload,
    ShellClosePayload,
    ExecPayload,
    ExecResultPayload,
    FileUploadPayload,
    FileUploadResultPayload,
    FileDownloadPayload,
    FileDownloadResultPayload,
    FileChunkPayload,
    FileDonePayload,
    BatchUploadPayload,
    BatchUploadResultPayload,
    BatchDownloadPayload,
    BatchDownloadResultPayload,
    BatchChunkPayload,
    BatchDonePayload,
    PingPayload,
    PongPayload,
    ErrorPayload,
    ClosePayload
>;

/**
 * @brief 协议消息结构
 *
 * 所有通信的基本单位。
 * 线格式：[1字节类型][8字节ID][8字节时间戳][JSON载荷]
 */
struct Message {
    MsgType type;       ///< 消息类型
    uint64_t id = 0;    ///< 请求ID（匹配请求/响应）
    uint64_t ts = 0;    ///< 时间戳（毫秒）
    Payload payload;    ///< 类型安全的载荷
};

}  // namespace serctrl::protocol
