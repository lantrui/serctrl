#pragma once

/**
 * @file updater.hpp
 * @brief 热更新模块
 *
 * 支持从 GitHub Releases 下载新版本并替换当前二进制。
 * 支持指定版本更新和版本回滚。
 *
 * 更新流程：
 * 1. 获取当前版本（~/.serctrl/version）
 * 2. 查询 GitHub Releases 可用版本
 * 3. 下载目标版本二进制
 * 4. 验证 SHA-256 校验和
 * 5. 备份当前二进制
 * 6. 原子替换
 *
 * 回滚流程：
 * 1. 从 ~/.serctrl/backup/ 恢复旧版本
 */

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace serctrl::update {

/**
 * @brief 版本信息
 *
 * 从 GitHub Releases API 获取的版本信息。
 */
struct ReleaseVersion {
    std::string tag;                    ///< 版本标签（如 "v1.0.0"）
    std::string name;                   ///< 版本名称
    std::string body;                   ///< 更新说明
    std::vector<std::string> assets;    ///< 下载资源列表
};

/**
 * @brief 更新管理器
 *
 * 管理二进制的自动更新和回滚。
 *
 * 用法：
 * @code
 *   Updater updater("lantrui", "serctrl");
 *   updater.updateTo("1.2.0");  // 更新到指定版本
 *   updater.rollback();          // 回滚
 * @endcode
 */
class Updater {
public:
    /**
     * @brief 构造更新管理器
     * @param repoOwner GitHub 仓库所有者
     * @param repoName GitHub 仓库名
     */
    Updater(const std::string& repoOwner, const std::string& repoName);

    /**
     * @brief 获取当前版本
     * @return 版本字符串（如 "0.1.0"）
     */
    std::string currentVersion() const;

    /**
     * @brief 列出所有可用版本
     * @return 版本列表（从新到旧）
     */
    std::vector<ReleaseVersion> listVersions() const;

    /**
     * @brief 获取最新版本
     * @return 最新版本信息
     */
    ReleaseVersion latestVersion() const;

    /**
     * @brief 更新到指定版本
     * @param version 目标版本（空字符串表示最新版）
     * @return 是否成功
     */
    bool updateTo(const std::string& version = "");

    /**
     * @brief 回滚到上一个版本
     * @return 是否成功
     */
    bool rollback();

    /**
     * @brief 设置进度回调
     * @param callback 回调函数 (百分比, 阶段描述)
     */
    void setProgressCallback(std::function<void(int percent, const std::string& stage)> callback);

private:
    /**
     * @brief 构造下载 URL
     * @param version 版本号
     * @return 下载 URL
     */
    std::string downloadUrl(const std::string& version) const;

    /**
     * @brief 验证文件校验和
     * @param file 文件路径
     * @param expected 期望的 SHA-256
     * @return 是否匹配
     */
    bool verifyChecksum(const std::filesystem::path& file, const std::string& expected);

    /**
     * @brief 备份当前二进制
     * @return 是否成功
     */
    bool backupCurrent();

    /**
     * @brief 替换二进制
     * @param newBinary 新二进制路径
     * @return 是否成功
     */
    bool replaceBinary(const std::filesystem::path& newBinary);

    std::string repoOwner_;             ///< GitHub 仓库所有者
    std::string repoName_;              ///< GitHub 仓库名
    std::filesystem::path installDir_;  ///< 安装目录（~/.serctrl/）
    std::filesystem::path backupDir_;   ///< 备份目录
    std::function<void(int, const std::string&)> onProgress_;  ///< 进度回调
};

}  // namespace serctrl::update
