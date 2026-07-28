/**
 * @file batch.cpp
 * @brief 批量文件传输实现
 *
 * 支持多文件批量上传/下载，每个文件独立分块。
 *
 * 上传流程：
 * 1. startUpload() 初始化批量状态
 * 2. getNextChunk() 获取下一个分块信息
 * 3. markChunkSent() 标记分块已发送
 * 4. 重复直到所有文件完成
 *
 * 下载流程：
 * 1. startDownload() 初始化批量状态
 * 2. writeChunk() 写入分块数据
 * 3. isComplete() 检查是否完成
 *
 * 分块大小：64KB（65536字节）
 */

#include "serctrl/transfer/batch.hpp"
#include "serctrl/util/error.hpp"

#include <fstream>
#include <sodium.h>

namespace serctrl::transfer {

BatchTransfer::BatchTransfer() = default;

std::optional<BatchState> BatchTransfer::startUpload(const std::string& requestId,
                                                     const std::vector<BatchFileSpec>& files) {
    BatchState state;
    state.requestId = requestId;
    state.files = files;
    state.currentFile = 0;
    state.currentChunk = 0;
    state.done = false;

    uploads_[requestId] = state;
    return uploads_[requestId];
}

std::optional<BatchState> BatchTransfer::startDownload(const std::string& requestId,
                                                       const std::vector<std::string>& remotePaths) {
    BatchState state;
    state.requestId = requestId;
    for (const auto& path : remotePaths) {
        state.files.push_back({path, path});
    }
    state.currentFile = 0;
    state.currentChunk = 0;
    state.done = false;

    downloads_[requestId] = state;
    return downloads_[requestId];
}

std::optional<BatchFileSpec> BatchTransfer::getNextChunk(const std::string& requestId,
                                                          uint32_t& chunkIndex,
                                                          uint32_t& totalChunks) {
    auto it = uploads_.find(requestId);
    if (it == uploads_.end() || it->second.done) {
        return std::nullopt;
    }

    auto& state = it->second;
    // 检查是否所有文件已完成
    if (state.currentFile >= state.files.size()) {
        state.done = true;
        return std::nullopt;
    }

    const auto& file = state.files[state.currentFile];
    std::filesystem::path path(file.localPath);

    // 检查文件是否存在
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    // 计算分块信息
    constexpr size_t CHUNK_SIZE = 64 * 1024;
    uint64_t fileSize = std::filesystem::file_size(path);
    totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    chunkIndex = state.currentChunk;

    return BatchFileSpec{file.localPath, file.remotePath};
}

void BatchTransfer::markChunkSent(const std::string& requestId) {
    auto it = uploads_.find(requestId);
    if (it == uploads_.end()) {
        return;
    }

    auto& state = it->second;
    state.currentChunk++;

    // 检查当前文件是否发送完成
    constexpr size_t CHUNK_SIZE = 64 * 1024;
    const auto& file = state.files[state.currentFile];
    uint64_t fileSize = std::filesystem::file_size(file.localPath);
    uint32_t totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    if (state.currentChunk >= totalChunks) {
        // 当前文件完成，切换到下一个文件
        state.currentFile++;
        state.currentChunk = 0;
    }

    // 检查所有文件是否完成
    if (state.currentFile >= state.files.size()) {
        state.done = true;
    }
}

void BatchTransfer::writeChunk(const std::string& requestId, uint32_t fileIndex,
                                uint32_t chunkIndex, std::span<const uint8_t> data) {
    auto it = downloads_.find(requestId);
    if (it == downloads_.end()) {
        return;
    }

    // 按文件索引组织缓冲区
    auto& buffers = downloadBuffers_[requestId];
    if (fileIndex >= buffers.size()) {
        buffers.resize(fileIndex + 1);
    }

    auto& fileBuffer = buffers[fileIndex];
    // 调整缓冲区大小以容纳分块
    if (chunkIndex * 64 * 1024 + data.size() > fileBuffer.size()) {
        fileBuffer.resize(chunkIndex * 64 * 1024 + data.size());
    }

    // 将分块数据复制到正确位置
    std::copy(data.begin(), data.end(),
              fileBuffer.begin() + chunkIndex * 64 * 1024);
}

bool BatchTransfer::isComplete(const std::string& requestId) const {
    // 检查上传状态
    auto it = uploads_.find(requestId);
    if (it != uploads_.end()) {
        return it->second.done;
    }

    // 检查下载状态
    auto dit = downloads_.find(requestId);
    if (dit != downloads_.end()) {
        return dit->second.done;
    }

    return false;
}

std::string BatchTransfer::computeSha256(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    // 使用 Blake2b 计算文件哈希
    unsigned char hash[crypto_generichash_BYTES];
    crypto_generichash_state state;
    crypto_generichash_init(&state, nullptr, 0, crypto_generichash_BYTES);

    // 分块读取计算哈希
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        crypto_generichash_update(&state, reinterpret_cast<const unsigned char*>(buffer), file.gcount());
    }
    // 处理最后一个不完整的块
    crypto_generichash_update(&state, reinterpret_cast<const unsigned char*>(buffer), file.gcount());

    crypto_generichash_final(&state, hash, crypto_generichash_BYTES);

    // 转换为十六进制字符串
    char hex[crypto_generichash_BYTES * 2 + 1];
    for (int i = 0; i < crypto_generichash_BYTES; ++i) {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }

    return std::string(hex, crypto_generichash_BYTES * 2);
}

}  // namespace serctrl::transfer
