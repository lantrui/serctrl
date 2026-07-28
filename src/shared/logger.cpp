/**
 * @file logger.cpp
 * @brief 日志系统实现
 *
 * 线程安全的单例日志系统。
 * 支持四级日志：Debug、Info、Warn、Error。
 *
 * 输出格式：
 * [2024-01-01 12:00:00] [INFO] prefix: message
 *
 * 使用方式：
 * - 通过 LOG_DEBUG/INFO/WARN/ERROR 宏调用
 * - 可自定义前缀（setPrefix）
 * - 可设置日志级别过滤（setLevel）
 */

#include "serctrl/util/logger.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace serctrl {

Logger& Logger::instance() {
    // 单例模式
    static Logger logger;
    return logger;
}

void Logger::setLevel(LogLevel level) {
    level_ = level;
}

void Logger::setPrefix(const std::string& prefix) {
    prefix_ = prefix;
}

void Logger::debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Debug, fmt, args);
    va_end(args);
}

void Logger::info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Info, fmt, args);
    va_end(args);
}

void Logger::warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Warn, fmt, args);
    va_end(args);
}

void Logger::error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Error, fmt, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char* fmt, va_list args) {
    // 过滤低于当前级别的日志
    if (level < level_) {
        return;
    }

    // 获取日志级别字符串
    const char* levelStr;
    switch (level) {
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info: levelStr = "INFO"; break;
        case LogLevel::Warn: levelStr = "WARN"; break;
        case LogLevel::Error: levelStr = "ERROR"; break;
        default: levelStr = "???"; break;
    }

    // 获取时间戳
    time_t now = time(nullptr);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 输出到 stderr
    fprintf(stderr, "[%s] [%s] %s: ", timeBuf, levelStr, prefix_.c_str());
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

}  // namespace serctrl
