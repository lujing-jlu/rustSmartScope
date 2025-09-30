#include "infrastructure/logging/log_formatter.h"
#include <QThread>

// 构造函数
LogFormatter::LogFormatter(FormatFunction formatFunction)
{
    if (formatFunction) {
        m_formatFunction = formatFunction;
    } else {
        m_formatFunction = getDefaultFormatter();
    }
}

// 析构函数
LogFormatter::~LogFormatter()
{
}

// 设置格式化函数
void LogFormatter::setFormatFunction(FormatFunction formatFunction)
{
    if (formatFunction) {
        m_formatFunction = formatFunction;
    }
}

// 格式化日志消息
QString LogFormatter::format(LogLevel level, const QString& message, const QDateTime& timestamp, 
                           const QString& file, int line, const QString& function) const
{
    return m_formatFunction(level, message, timestamp, file, line, function);
}

// 获取默认格式化函数
LogFormatter::FormatFunction LogFormatter::getDefaultFormatter()
{
    return [](LogLevel level, const QString& message, const QDateTime& timestamp, 
              const QString& file, int line, const QString& function) -> QString {
        QString result;
        
        // 添加时间戳
        result += timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz ");
        
        // 添加日志级别
        result += "[" + Logger::levelToString(level) + "] ";
        
        // 添加文件名、行号和函数名（如果有）
        if (!file.isEmpty()) {
            result += file;
            if (line > 0) {
                result += ":" + QString::number(line);
            }
            result += " ";
        }
        
        if (!function.isEmpty()) {
            result += function + "() ";
        }
        
        // 添加日志消息
        result += message;
        
        return result;
    };
}

// 获取简单格式化函数
LogFormatter::FormatFunction LogFormatter::getSimpleFormatter()
{
    return [](LogLevel level, const QString& message, const QDateTime& timestamp, 
              const QString& file, int line, const QString& function) -> QString {
        Q_UNUSED(file);
        Q_UNUSED(line);
        Q_UNUSED(function);
        
        QString result;
        
        // 添加时间戳
        result += timestamp.toString("hh:mm:ss ");
        
        // 添加日志级别
        result += "[" + Logger::levelToString(level) + "] ";
        
        // 添加日志消息
        result += message;
        
        return result;
    };
}

// 获取详细格式化函数
LogFormatter::FormatFunction LogFormatter::getDetailedFormatter()
{
    return [](LogLevel level, const QString& message, const QDateTime& timestamp, 
              const QString& file, int line, const QString& function) -> QString {
        QString result;
        
        // 添加时间戳
        result += timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz ");
        
        // 添加日志级别
        result += "[" + Logger::levelToString(level) + "] ";
        
        // 添加线程ID
        result += "[Thread:" + QString::number((quintptr)QThread::currentThreadId()) + "] ";
        
        // 添加文件名、行号和函数名（如果有）
        if (!file.isEmpty()) {
            result += "[" + file;
            if (line > 0) {
                result += ":" + QString::number(line);
            }
            result += "] ";
        }
        
        if (!function.isEmpty()) {
            result += "[" + function + "()] ";
        }
        
        // 添加日志消息
        result += message;
        
        return result;
    };
} 