/**
 * @file updater.cpp
 * @brief 热更新实现（存根）
 *
 * 从 GitHub Releases 下载并更新 serctrl。
 * 当前为存根实现，待完整实现。
 *
 * 更新流程：
 * 1. 从 GitHub API 获取版本列表
 * 2. 用户选择目标版本
 * 3. 下载对应平台的二进制文件
 * 4. 验证 SHA-256 校验和
 * 5. 备份当前版本
 * 6. 替换二进制文件
 * 7. 如果失败，回滚到备份版本
 */

#include "serctrl/update/updater.hpp"
#include "serctrl/util/logger.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace serctrl::update {

Updater::Updater(const std::string& repoOwner, const std::string& repoName)
    : repoOwner_(repoOwner)
    , repoName_(repoName)
    , installDir_(std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / ".serctrl")
    , backupDir_(installDir_ / "backup") {
}

std::string Updater::currentVersion() const {
    // 读取版本文件
    auto versionFile = installDir_ / "version";
    if (!std::filesystem::exists(versionFile)) {
        return "0.0.0";
    }

    std::ifstream file(versionFile);
    std::string version;
    std::getline(file, version);
    return version;
}

std::vector<ReleaseVersion> Updater::listVersions() const {
    // TODO: 从 GitHub API 获取版本列表
    return {};
}

ReleaseVersion Updater::latestVersion() const {
    // TODO: 从 GitHub API 获取最新版本
    return {};
}

bool Updater::updateTo(const std::string& version) {
    LOG_INFO("Updating to version %s", version.empty() ? "latest" : version.c_str());

    if (onProgress_) {
        onProgress_(0, "Checking for updates");
    }

    // TODO: 实现更新逻辑
    // 1. 从 GitHub 获取 release 信息
    // 2. 下载二进制文件
    // 3. 验证校验和
    // 4. 备份当前版本
    // 5. 替换二进制文件

    return false;
}

bool Updater::rollback() {
    LOG_INFO("Rolling back to previous version");

    auto backupBinary = backupDir_ / "serctrl";
    if (!std::filesystem::exists(backupBinary)) {
        LOG_ERROR("No backup found");
        return false;
    }

    // TODO: 从备份恢复
    return false;
}

void Updater::setProgressCallback(std::function<void(int percent, const std::string& stage)> callback) {
    onProgress_ = std::move(callback);
}

std::string Updater::downloadUrl(const std::string& version) const {
    // 构建 GitHub Releases 下载链接
    return "https://github.com/" + repoOwner_ + "/" + repoName_ +
           "/releases/download/" + version + "/serctrl-" + version + ".tar.gz";
}

bool Updater::verifyChecksum(const std::filesystem::path& file, const std::string& expected) {
    // TODO: 实现 SHA-256 校验和验证
    return false;
}

bool Updater::backupCurrent() {
    // TODO: 备份当前二进制文件
    return false;
}

bool Updater::replaceBinary(const std::filesystem::path& newBinary) {
    // TODO: 使用原子重命名替换二进制文件
    return false;
}

}  // namespace serctrl::update
