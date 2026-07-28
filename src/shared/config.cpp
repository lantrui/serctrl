/**
 * @file config.cpp
 * @brief 配置管理实现
 *
 * 管理 serctrl 客户端的本地配置。
 * 配置存储在 ~/.serctrl/config.json。
 *
 * config.json 格式：
 * {
 *   "hosts": {
 *     "myserver": {
 *       "user": "admin",
 *       "host": "192.168.1.100",
 *       "port": 8080,
 *       "token": "..."
 *     }
 *   }
 * }
 *
 * 支持 SSH 风格的连接目标：user@host:port
 */

#include "serctrl/config/config.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace serctrl::config {

Config::Config() : Config(std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / ".serctrl") {}

Config::Config(const std::filesystem::path& configDir)
    : configDir_(configDir)
    , configFile_(configDir / "config.json") {
    // 构造时加载现有配置
    load();
}

std::optional<HostConfig> Config::getHost(const std::string& name) const {
    auto it = data_.hosts.find(name);
    if (it == data_.hosts.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Config::setHost(const std::string& name, const HostConfig& host) {
    data_.hosts[name] = host;
    save();
}

bool Config::removeHost(const std::string& name) {
    auto it = data_.hosts.find(name);
    if (it == data_.hosts.end()) {
        return false;
    }
    data_.hosts.erase(it);
    save();
    return true;
}

std::vector<std::string> Config::listHosts() const {
    std::vector<std::string> names;
    names.reserve(data_.hosts.size());
    for (const auto& [name, _] : data_.hosts) {
        names.push_back(name);
    }
    return names;
}

void Config::load() {
    // 如果文件不存在，跳过
    if (!std::filesystem::exists(configFile_)) {
        return;
    }

    try {
        std::ifstream file(configFile_);
        nlohmann::json j;
        file >> j;

        // 解析主机配置
        if (j.contains("hosts")) {
            for (const auto& [name, host] : j["hosts"].items()) {
                HostConfig hc;
                hc.user = host.value("user", "");
                hc.host = host.value("host", "");
                hc.port = host.value("port", 8080);
                hc.token = host.value("token", "");
                data_.hosts[name] = hc;
            }
        }
    } catch (...) {
        // 解析错误时忽略，使用空数据
    }
}

void Config::save() const {
    // 确保目录存在
    std::filesystem::create_directories(configDir_);

    nlohmann::json j;
    j["hosts"] = nlohmann::json::object();

    // 序列化主机配置
    for (const auto& [name, host] : data_.hosts) {
        nlohmann::json h;
        h["user"] = host.user;
        h["host"] = host.host;
        h["port"] = host.port;
        h["token"] = host.token;
        j["hosts"][name] = h;
    }

    // 写入格式化的 JSON
    std::ofstream file(configFile_);
    file << j.dump(2);
}

ParsedTarget Config::parseTarget(const std::string& target) {
    ParsedTarget result;
    std::string rest = target;

    // 解析 user@host:port 格式
    // 1. 提取用户名（@ 之前的部分）
    auto atPos = rest.find('@');
    if (atPos != std::string::npos) {
        result.user = rest.substr(0, atPos);
        rest = rest.substr(atPos + 1);
    }

    // 2. 提取端口号（: 之后的部分）
    auto colonPos = rest.find(':');
    if (colonPos != std::string::npos) {
        result.host = rest.substr(0, colonPos);
        try {
            result.port = std::stoi(rest.substr(colonPos + 1));
        } catch (...) {
            // 解析失败使用默认端口
            result.port = 8080;
        }
    } else {
        result.host = rest;
    }

    return result;
}

}  // namespace serctrl::config
