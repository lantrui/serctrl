#pragma once

/**
 * @file config.hpp
 * @brief 配置管理模块
 *
 * 管理客户端配置文件（~/.serctrl/config.json）。
 * 存储已保存的主机连接信息。
 *
 * 配置文件格式：
 * @code
 * {
 *   "hosts": {
 *     "myhost": {
 *       "user": "admin",
 *       "host": "192.168.1.100",
 *       "port": 8080,
 *       "token": "..."
 *     }
 *   }
 * }
 * @endcode
 */

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace serctrl::config {

/**
 * @brief 主机配置
 *
 * 保存一个远程主机的连接信息。
 */
struct HostConfig {
    std::string user;           ///< 用户名
    std::string host;           ///< 主机地址
    uint16_t port = 8080;       ///< 端口号
    std::string token;          ///< 认证令牌（如有主密码则加密存储）
};

/**
 * @brief 应用配置
 *
 * 所有已保存主机的集合。
 */
struct AppConfig {
    std::unordered_map<std::string, HostConfig> hosts;  ///< 主机名 → 配置映射
};

/**
 * @brief 解析后的目标地址
 *
 * 从 "user@host:port" 格式解析出的各部分。
 */
struct ParsedTarget {
    std::string user;       ///< 用户名（可能为空）
    std::string host;       ///< 主机地址
    uint16_t port = 8080;   ///< 端口号
};

/**
 * @brief 配置管理器
 *
 * 管理 ~/.serctrl/ 目录下的配置文件。
 * 支持增删改查主机配置。
 *
 * 用法：
 * @code
 *   Config config;
 *   config.setHost("myhost", {"admin", "192.168.1.100", 8080, "token"});
 *   auto host = config.getHost("myhost");
 * @endcode
 */
class Config {
public:
    /** @brief 使用默认配置目录（~/.serctrl/） */
    Config();

    /**
     * @brief 指定配置目录
     * @param configDir 配置目录路径
     */
    explicit Config(const std::filesystem::path& configDir);

    /**
     * @brief 获取主机配置
     * @param name 主机名
     * @return 主机配置，不存在返回 std::nullopt
     */
    std::optional<HostConfig> getHost(const std::string& name) const;

    /**
     * @brief 设置主机配置
     * @param name 主机名
     * @param host 主机配置
     */
    void setHost(const std::string& name, const HostConfig& host);

    /**
     * @brief 删除主机配置
     * @param name 主机名
     * @return 是否成功
     */
    bool removeHost(const std::string& name);

    /**
     * @brief 列出所有主机名
     * @return 主机名列表
     */
    std::vector<std::string> listHosts() const;

    void load();        ///< 从文件加载配置
    void save() const;  ///< 保存配置到文件

    /** @brief 获取配置目录路径 */
    const std::filesystem::path& configDir() const { return configDir_; }
    /** @brief 获取配置文件路径 */
    const std::filesystem::path& configFile() const { return configFile_; }

    /**
     * @brief 解析目标地址字符串
     * @param target 格式：[user@]host[:port]
     * @return 解析结果
     *
     * 示例：
     * - "admin@192.168.1.100:8080" → {user="admin", host="192.168.1.100", port=8080}
     * - "root@server" → {user="root", host="server", port=8080}
     * - "myhost:9090" → {user="", host="myhost", port=9090}
     */
    static ParsedTarget parseTarget(const std::string& target);

private:
    std::filesystem::path configDir_;   ///< 配置目录
    std::filesystem::path configFile_;  ///< 配置文件
    AppConfig data_;                    ///< 配置数据
};

}  // namespace serctrl::config
