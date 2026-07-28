/**
 * @file file_transfer.cpp
 * @brief 单文件传输实现
 *
 * 支持大文件分块上传/下载，使用 SHA-256 校验完整性。
 *
 * 上传流程：
 * 1. startUpload() 初始化上传状态
 * 2. readChunk() 读取分块数据
 * 3. 发送完成后调用 markUploaded()
 *
 * 下载流程：
 * 1. startDownload() 初始化下载状态
 * 2. writeChunk() 接收分块数据
 * 3. isDownloadComplete() 检查是否完成
 *
 * 完整性校验：
 * - computeSha256() 使用 Blake2b（libsodium）计算文件哈希
 */

#include "serctrl/transfer/file_transfer.hpp"
#include "serctrl/util/error.hpp"

#include <fstream>
#include <sodium.h>

namespace serctrl::transfer {

FileTransfer::FileTransfer() = default;

std::optional<UploadState> FileTransfer::startUpload(const std::string& requestId,
                                                     const std::filesystem::path& localPath,
                                                     const std::string& remotePath) {
    // 检查本地文件是否存在
    if (!std::filesystem::exists(localPath)) {
        return std::nullopt;
    }

    UploadState state;
    state.requestId = requestId;
    state.localPath = localPath;
    state.remotePath = remotePath;
    state.totalSize = std::filesystem::file_size(localPath);
    state.sent = 0;
    state.done = false;

    uploads_[requestId] = state;
    return uploads_[requestId];
}

std::vector<uint8_t> FileTransfer::readChunk(const std::string& requestId, size_t chunkSize) {
    auto it = uploads_.find(requestId);
    if (it == uploads_.end()) {
        return {};
    }

    auto& state = it->second;
    // 打开文件并定位到已发送位置
    std::ifstream file(state.localPath, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    // 读取指定大小的分块
    file.seekg(state.sent);
    std::vector<uint8_t> chunk(chunkSize);
    file.read(reinterpret_cast<char*>(chunk.data()), chunkSize);
    chunk.resize(file.gcount());  // 调整为实际读取大小

    // 更新进度
    state.sent += chunk.size();
    if (state.sent >= state.totalSize) {
        state.done = true;
    }

    return chunk;
}

void FileTransfer::markUploaded(const std::string& requestId) {
    // 标记上传完成，清除状态
    uploads_.erase(requestId);
}

std::optional<DownloadState> FileTransfer::startDownload(const std::string& requestId,
                                                         const std::string& remotePath,
                                                         const std::filesystem::path& localPath) {
    DownloadState state;
    state.requestId = requestId;
    state.remotePath = remotePath;
    state.localPath = localPath;
    state.totalSize = 0;
    state.received = 0;
    state.done = false;

    downloads_[requestId] = state;
    return downloads_[requestId];
}

void FileTransfer::writeChunk(const std::string& requestId, std::span<const uint8_t> data) {
    auto it = downloads_.find(requestId);
    if (it == downloads_.end()) {
        return;
    }

    auto& state = it->second;
    // 将分块数据追加到缓冲区
    state.buffer.insert(state.buffer.end(), data.begin(), data.end());
    state.received += data.size();
}

bool FileTransfer::isDownloadComplete(const std::string& requestId) const {
    auto it = downloads_.find(requestId);
    if (it == downloads_.end()) {
        return false;
    }
    return it->second.done;
}

std::string FileTransfer::computeSha256(const std::filesystem::path& path) {
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
