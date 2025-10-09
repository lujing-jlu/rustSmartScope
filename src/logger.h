#pragma once

#include <string>
#include <sstream>

extern "C" {
    // C FFI日志函数声明
    enum CLogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4
    };

    int smartscope_log(CLogLevel level, const char* module, const char* message);
    int smartscope_log_qml(CLogLevel level, const char* message);
    int smartscope_set_log_level(CLogLevel level);
}

/// C++日志封装类
class Logger {
public:
    enum Level {
        Trace = TRACE,
        Debug = DEBUG,
        Info = INFO,
        Warn = WARN,
        Error = ERROR
    };

    /// 设置全局日志级别
    static void setLevel(Level level) {
        smartscope_set_log_level(static_cast<CLogLevel>(level));
    }

    /// 记录Trace级别日志
    template<typename... Args>
    static void trace(const std::string& module, Args&&... args) {
        log(Trace, module, args...);
    }

    /// 记录Debug级别日志
    template<typename... Args>
    static void debug(const std::string& module, Args&&... args) {
        log(Debug, module, args...);
    }

    /// 记录Info级别日志
    template<typename... Args>
    static void info(const std::string& module, Args&&... args) {
        log(Info, module, args...);
    }

    /// 记录Warn级别日志
    template<typename... Args>
    static void warn(const std::string& module, Args&&... args) {
        log(Warn, module, args...);
    }

    /// 记录Error级别日志
    template<typename... Args>
    static void error(const std::string& module, Args&&... args) {
        log(Error, module, args...);
    }

    /// QML专用日志函数
    template<typename... Args>
    static void qml(Level level, Args&&... args) {
        std::ostringstream oss;
        ((oss << args), ...);
        smartscope_log_qml(static_cast<CLogLevel>(level), oss.str().c_str());
    }

private:
    /// 通用日志函数
    template<typename... Args>
    static void log(Level level, const std::string& module, Args&&... args) {
        std::ostringstream oss;
        ((oss << args), ...);
        smartscope_log(static_cast<CLogLevel>(level), module.c_str(), oss.str().c_str());
    }
};

/// 便捷宏定义
#define LOG_TRACE(module, ...) Logger::trace(module, __VA_ARGS__)
#define LOG_DEBUG(module, ...) Logger::debug(module, __VA_ARGS__)
#define LOG_INFO(module, ...)  Logger::info(module, __VA_ARGS__)
#define LOG_WARN(module, ...)  Logger::warn(module, __VA_ARGS__)
#define LOG_ERROR(module, ...) Logger::error(module, __VA_ARGS__)

/// QML日志宏
#define QML_TRACE(...) Logger::qml(Logger::Trace, __VA_ARGS__)
#define QML_DEBUG(...) Logger::qml(Logger::Debug, __VA_ARGS__)
#define QML_INFO(...)  Logger::qml(Logger::Info, __VA_ARGS__)
#define QML_WARN(...)  Logger::qml(Logger::Warn, __VA_ARGS__)
#define QML_ERROR(...) Logger::qml(Logger::Error, __VA_ARGS__)