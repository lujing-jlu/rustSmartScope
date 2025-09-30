#ifndef LOG_APPENDER_H
#define LOG_APPENDER_H

#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <memory>
#include "infrastructure/logging/logger.h"
#include "infrastructure/logging/log_formatter.h"

/**
 * @brief 日志输出目标抽象基类
 * 
 * 负责将日志输出到不同的目标，如控制台、文件等
 */
class LogAppender
{
public:
    /**
     * @brief 构造函数
     * @param formatter 日志格式化器，为空时使用默认格式
     */
    LogAppender(std::shared_ptr<LogFormatter> formatter = nullptr);
    
    /**
     * @brief 虚析构函数
     */
    virtual ~LogAppender();
    
    /**
     * @brief 设置日志格式化器
     * @param formatter 日志格式化器
     */
    void setFormatter(std::shared_ptr<LogFormatter> formatter);
    
    /**
     * @brief 获取日志格式化器
     * @return 日志格式化器
     */
    std::shared_ptr<LogFormatter> getFormatter() const;
    
    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLogLevel(LogLevel level);
    
    /**
     * @brief 获取日志级别
     * @return 日志级别
     */
    LogLevel getLogLevel() const;
    
    /**
     * @brief 输出日志
     * @param level 日志级别
     * @param message 日志消息
     * @param timestamp 时间戳
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    virtual void append(LogLevel level, const QString& message, const QDateTime& timestamp, 
                       const QString& file, int line, const QString& function) = 0;

protected:
    // 日志格式化器
    std::shared_ptr<LogFormatter> m_formatter;
    
    // 日志级别
    LogLevel m_logLevel;
    
    // 互斥锁，保证线程安全
    mutable QMutex m_mutex;
};

/**
 * @brief 控制台日志输出目标
 * 
 * 将日志输出到控制台
 */
class ConsoleAppender : public LogAppender
{
public:
    /**
     * @brief 构造函数
     * @param formatter 日志格式化器，为空时使用默认格式
     */
    ConsoleAppender(std::shared_ptr<LogFormatter> formatter = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ConsoleAppender() override;
    
    /**
     * @brief 输出日志到控制台
     * @param level 日志级别
     * @param message 日志消息
     * @param timestamp 时间戳
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void append(LogLevel level, const QString& message, const QDateTime& timestamp, 
               const QString& file, int line, const QString& function) override;

private:
    // 日志级别到颜色的映射
    QMap<LogLevel, QString> m_levelColors;
};

/**
 * @brief 文件日志输出目标
 * 
 * 将日志输出到文件
 */
class FileAppender : public LogAppender
{
public:
    /**
     * @brief 构造函数
     * @param filePath 日志文件路径
     * @param formatter 日志格式化器，为空时使用默认格式
     */
    FileAppender(const QString& filePath, std::shared_ptr<LogFormatter> formatter = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~FileAppender() override;
    
    /**
     * @brief 输出日志到文件
     * @param level 日志级别
     * @param message 日志消息
     * @param timestamp 时间戳
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void append(LogLevel level, const QString& message, const QDateTime& timestamp, 
               const QString& file, int line, const QString& function) override;
    
    /**
     * @brief 设置日志文件路径
     * @param filePath 文件路径
     * @return 是否成功设置
     */
    bool setFilePath(const QString& filePath);
    
    /**
     * @brief 获取日志文件路径
     * @return 文件路径
     */
    QString getFilePath() const;
    
    /**
     * @brief 设置是否自动刷新
     * @param autoFlush 是否自动刷新
     */
    void setAutoFlush(bool autoFlush);
    
    /**
     * @brief 获取是否自动刷新
     * @return 是否自动刷新
     */
    bool getAutoFlush() const;
    
    /**
     * @brief 刷新日志文件
     */
    void flush();

private:
    // 日志文件路径
    QString m_filePath;
    
    // 日志文件
    QFile m_file;
    
    // 文本流
    QTextStream m_textStream;
    
    // 是否自动刷新
    bool m_autoFlush;
};

#endif // LOG_APPENDER_H 