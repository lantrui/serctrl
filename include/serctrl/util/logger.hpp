#pragma once

/**
 * @file logger.hpp
 * @brief 日志模块
 *
 * 简单的日志库，支持四个级别：Debug、Info、Warn、Error。
 * 输出到 stderr，格式：[时间] [级别] 前缀: 消息
 *
 * 用法：
 * @code
 *   LOG_INFO("Server started on port %d", 8080);
 *   LOG_ERROR("Connection failed: %s", error.c_str());
 * @endcode
 */

#include <cstdint>
#include <cstdio>
#include <string>

namespace serctrl {

/**
 * @brief 日志级别
 *
 * 从低到高：Debug < Info < Warn < Error
 * 设置某个级别后，低于该级别的日志会被忽略。
 */
enum class LogLevel : uint8_t {
    Debug = 0,  ///< 调试信息（最详细）
    Info = 1,   ///< 一般信息（默认）
    Warn = 2,   ///< 警告
    Error = 3,  ///< 错误
};

/**
 * @brief 日志器（单例）
 *
 * 线程安全的日志输出。
 * 使用 spdlog 风格的宏调用。
 */
class Logger {
public:
    /** @brief 获取单例实例 */
    static Logger& instance();

    /**
     * @brief 设置日志级别
     * @param level 最低输出级别
     */
    void setLevel(LogLevel level);

    /**
     * @brief 设置日志前缀
     * @param prefix 前缀字符串（如 "serctrld"）
     */
    void setPrefix(const std::string& prefix);

    /**
     * @brief 输出调试日志
     * @param fmt printf 风格格式字符串
     */
    void debug(const char* fmt, ...);

    /**
     * @brief 输出一般日志
     * @param fmt printf 风格格式字符串
     */
    void info(const char* fmt, ...);

    /**
     * @brief 输出警告日志
     * @param fmt printf 风格格式字符串
     */
    void warn(const char* fmt, ...);

    /**
     * @brief 输出错误日志
     * @param fmt printf 风格格式字符串
     */
    void error(const char* fmt, ...);

private:
    Logger() = default;

    /**
     * @brief 内部日志输出
     * @param level 日志级别
     * @param fmt 格式字符串
     * @param args 可变参数列表
     */
    void log(LogLevel level, const char* fmt, va_list args);

    LogLevel level_ = LogLevel::Info;       ///< 当前级别
    std::string prefix_ = "serctrl";        ///< 日志前缀
};

// ═══════════════════════════════════════════
// 便捷宏
// ═══════════════════════════════════════════

#define LOG_DEBUG(...) serctrl::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...)  serctrl::Logger::instance().info(__VA_ARGS__)
#define LOG_WARN(...)  serctrl::Logger::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) serctrl::Logger::instance().error(__VA_ARGS__)

}  // namespace serctrl
