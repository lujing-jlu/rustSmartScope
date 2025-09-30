#ifndef LOG_FORMATTER_H
#define LOG_FORMATTER_H

#include <QString>
#include <QDateTime>
#include <functional>
#include "infrastructure/logging/logger.h"

/**
 * @brief 日志格式化器类
 * 
 * 负责格式化日志消息，支持自定义格式
 */
class LogFormatter
{
public:
    /**
     * @brief 格式化函数类型
     * 接收日志级别、消息、时间戳、文件名、行号和函数名，返回格式化后的字符串
     */
    using FormatFunction = std::function<QString(LogLevel, const QString&, const QDateTime&, 
                                               const QString&, int, const QString&)>;
    
    /**
     * @brief 构造函数
     * @param formatFunction 自定义格式化函数，为空时使用默认格式
     */
    LogFormatter(FormatFunction formatFunction = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~LogFormatter();
    
    /**
     * @brief 设置格式化函数
     * @param formatFunction 格式化函数
     */
    void setFormatFunction(FormatFunction formatFunction);
    
    /**
     * @brief 格式化日志消息
     * @param level 日志级别
     * @param message 日志消息
     * @param timestamp 时间戳
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     * @return 格式化后的字符串
     */
    QString format(LogLevel level, const QString& message, const QDateTime& timestamp, 
                  const QString& file, int line, const QString& function) const;
    
    /**
     * @brief 获取默认格式化函数
     * @return 默认格式化函数
     */
    static FormatFunction getDefaultFormatter();
    
    /**
     * @brief 获取简单格式化函数（只包含时间和消息）
     * @return 简单格式化函数
     */
    static FormatFunction getSimpleFormatter();
    
    /**
     * @brief 获取详细格式化函数（包含所有信息）
     * @return 详细格式化函数
     */
    static FormatFunction getDetailedFormatter();

private:
    // 格式化函数
    FormatFunction m_formatFunction;
};

#endif // LOG_FORMATTER_H 