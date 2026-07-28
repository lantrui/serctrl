#pragma once

/**
 * @file file_transfer.hpp
 * @brief 单文件传输模块
 *
 * 管理单个文件的分块上传/下载。
 * 支持断点续传和 SHA-256 校验。
 *
 * 传输流程：
 * 1. 发起端：startUpload() → 循环 readChunk() → 发送 → markUploaded()
 * 2. 接收端：startDownload() → 循环 writeChunk() → 检查完整性
 * 3. 最后计算 SHA-256 校验和
 */

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace serctrl::transfer {

/** @brief 默认块大小（64KB） */
constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024;

/**
 * @brief 上传状态
 *
 * 跟踪单个文件的上传进度。
 */
struct UploadState {
    std::string requestId;              ///< 请求ID
    std::filesystem::path localPath;    ///< 本地文件路径
    std::string remotePath;             ///< 远端目标路径
    uint64_t totalSize = 0;             ///< 文件总大小
    uint64_t sent = 0;                  ///< 已发送字节数
    bool done = false;                  ///< 是否完成
};

/**
 * @brief 下载状态
 *
 * 跟踪单个文件的下载进度。
 */
struct DownloadState {
    std::string requestId;              ///< 请求ID
    std::string remotePath;             ///< 远端文件路径
    std::filesystem::path localPath;    ///< 本地目标路径
    uint64_t totalSize = 0;             ///< 文件总大小
    uint64_t received = 0;              ///< 已接收字节数
    std::vector<uint8_t> buffer;        ///< 数据缓冲区
    bool done = false;                  ///< 是否完成
};

/**
 * @brief 文件传输管理器
 *
 * 管理多个并发的文件传输任务。
 * 每个任务由 requestId 标识。
 *
 * 用法：
 * @code
 *   FileTransfer ft;
 *   auto state = ft.startUpload("req-1", "/local/file.txt", "/remote/file.txt");
 *   while (auto chunk = ft.readChunk("req-1")) {
 *       // 发送 chunk
 *   }
 *   auto hash = ft.computeSha256("/local/file.txt");
 * @endcode
 */
class FileTransfer {
public:
    /** @brief 数据块发送回调类型 */
    using SendChunkFn = std::function<void(const std::vector<uint8_t>&)>;

    FileTransfer();

    /**
     * @brief 开始上传
     * @param requestId 请求ID
     * @param localPath 本地文件路径
     * @param remotePath 远端目标路径
     * @return 上传状态，文件不存在返回 std::nullopt
     */
    std::optional<UploadState> startUpload(const std::string& requestId,
                                           const std::filesystem::path& localPath,
                                           const std::string& remotePath);

    /**
     * @brief 读取下一块数据
     * @param requestId 请求ID
     * @param chunkSize 块大小
     * @return 数据块，完成或出错返回空向量
     */
    std::vector<uint8_t> readChunk(const std::string& requestId, size_t chunkSize = DEFAULT_CHUNK_SIZE);

    /**
     * @brief 标记上传完成
     * @param requestId 请求ID
     */
    void markUploaded(const std::string& requestId);

    /**
     * @brief 开始下载
     * @param requestId 请求ID
     * @param remotePath 远端文件路径
     * @param localPath 本地目标路径
     * @return 下载状态
     */
    std::optional<DownloadState> startDownload(const std::string& requestId,
                                               const std::string& remotePath,
                                               const std::filesystem::path& localPath);

    /**
     * @brief 写入接收的数据块
     * @param requestId 请求ID
     * @param data 数据块
     */
    void writeChunk(const std::string& requestId, std::span<const uint8_t> data);

    /**
     * @brief 检查下载是否完成
     * @param requestId 请求ID
     * @return 是否完成
     */
    bool isDownloadComplete(const std::string& requestId) const;

    /**
     * @brief 计算文件 SHA-256 校验和
     * @param path 文件路径
     * @return 64字符十六进制字符串
     */
    std::string computeSha256(const std::filesystem::path& path);

private:
    std::unordered_map<std::string, UploadState> uploads_;      ///< 上传任务映射
    std::unordered_map<std::string, DownloadState> downloads_;  ///< 下载任务映射
};

}  // namespace serctrl::transfer
