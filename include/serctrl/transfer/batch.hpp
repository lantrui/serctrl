#pragma once

/**
 * @file batch.hpp
 * @brief 批量文件传输模块
 *
 * 管理多个文件的批量上传/下载。
 * 一次握手传输多个文件，减少连接开销。
 *
 * 传输流程：
 * 1. 发起端：startUpload() → 循环 getNextChunk() → markChunkSent()
 * 2. 接收端：startDownload() → 循环 writeChunk()
 * 3. 每个文件完成后计算 SHA-256 校验和
 */

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace serctrl::transfer {

/**
 * @brief 批量传输文件规格
 *
 * 描述一个文件的本地和远端路径。
 */
struct BatchFileSpec {
    std::string localPath;   ///< 本地路径
    std::string remotePath;  ///< 远端路径
};

/**
 * @brief 批量传输状态
 *
 * 跟踪批量传输的整体进度。
 */
struct BatchState {
    std::string requestId;                  ///< 请求ID
    std::vector<BatchFileSpec> files;       ///< 文件列表
    uint32_t currentFile = 0;               ///< 当前文件序号
    uint32_t currentChunk = 0;              ///< 当前块序号
    bool done = false;                      ///< 是否全部完成
};

/**
 * @brief 批量传输管理器
 *
 * 管理多个文件的批量传输任务。
 *
 * 用法：
 * @code
 *   BatchTransfer bt;
 *   std::vector<BatchFileSpec> files = {
 *       {"/local/a.txt", "/remote/a.txt"},
 *       {"/local/b.txt", "/remote/b.txt"}
 *   };
 *   bt.startUpload("req-1", files);
 *   uint32_t idx, total;
 *   while (auto spec = bt.getNextChunk("req-1", idx, total)) {
 *       // 读取并发送块
 *       bt.markChunkSent("req-1");
 *   }
 * @endcode
 */
class BatchTransfer {
public:
    BatchTransfer();

    /**
     * @brief 开始批量上传
     * @param requestId 请求ID
     * @param files 文件列表
     * @return 批量状态
     */
    std::optional<BatchState> startUpload(const std::string& requestId,
                                          const std::vector<BatchFileSpec>& files);

    /**
     * @brief 开始批量下载
     * @param requestId 请求ID
     * @param remotePaths 远端文件路径列表
     * @return 批量状态
     */
    std::optional<BatchState> startDownload(const std::string& requestId,
                                            const std::vector<std::string>& remotePaths);

    /**
     * @brief 获取下一个要发送的块
     * @param requestId 请求ID
     * @param chunkIndex [out] 当前块序号
     * @param totalChunks [out] 总块数
     * @return 文件规格，完成返回 std::nullopt
     */
    std::optional<BatchFileSpec> getNextChunk(const std::string& requestId,
                                               uint32_t& chunkIndex,
                                               uint32_t& totalChunks);

    /**
     * @brief 标记块已发送
     * @param requestId 请求ID
     */
    void markChunkSent(const std::string& requestId);

    /**
     * @brief 写入接收的数据块
     * @param requestId 请求ID
     * @param fileIndex 文件序号
     * @param chunkIndex 块序号
     * @param data 数据块
     */
    void writeChunk(const std::string& requestId, uint32_t fileIndex,
                    uint32_t chunkIndex, std::span<const uint8_t> data);

    /**
     * @brief 检查批量传输是否完成
     * @param requestId 请求ID
     * @return 是否完成
     */
    bool isComplete(const std::string& requestId) const;

    /**
     * @brief 计算文件 SHA-256 校验和
     * @param path 文件路径
     * @return 64字符十六进制字符串
     */
    std::string computeSha256(const std::filesystem::path& path);

private:
    std::unordered_map<std::string, BatchState> uploads_;   ///< 上传任务映射
    std::unordered_map<std::string, BatchState> downloads_; ///< 下载任务映射
    std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> downloadBuffers_;  ///< 下载缓冲区
};

}  // namespace serctrl::transfer
