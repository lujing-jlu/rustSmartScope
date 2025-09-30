#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMap>
#include <functional>
#include <memory>

// 前向声明
class LoggerDeleter;

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

/**
 * @brief 日志记录器类，采用单例模式
 * 
 * 负责记录应用程序的日志，支持多级别日志和多种输出目标
 */
class Logger : public QObject
{
    Q_OBJECT

    // 友元类，用于删除单例实例
    friend class LoggerDeleter;

public:
    /**
     * @brief 获取Logger单例实例
     * @return Logger单例引用
     */
    static Logger& instance();

    /**
     * @brief 初始化日志系统
     * @param logFilePath 日志文件路径，为空时使用默认路径
     * @param logLevel 日志级别，默认为INFO
     * @param enableConsole 是否启用控制台输出，默认为true
     * @param enableFile 是否启用文件输出，默认为true
     * @return 是否成功初始化
     */
    bool init(const QString& logFilePath = QString(), 
              LogLevel logLevel = LogLevel::INFO,
              bool enableConsole = true,
              bool enableFile = true);

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 获取当前日志级别
     * @return 日志级别
     */
    LogLevel getLogLevel() const;

    /**
     * @brief 设置是否启用控制台输出
     * @param enable 是否启用
     */
    void setConsoleEnabled(bool enable);

    /**
     * @brief 设置是否启用文件输出
     * @param enable 是否启用
     */
    void setFileEnabled(bool enable);

    /**
     * @brief 设置日志文件路径
     * @param filePath 文件路径
     * @return 是否成功设置
     */
    bool setLogFilePath(const QString& filePath);

    /**
     * @brief 获取日志文件路径
     * @return 文件路径
     */
    QString getLogFilePath() const;

    /**
     * @brief 记录调试级别日志
     * @param message 日志消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void debug(const QString& message, const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief 记录信息级别日志
     * @param message 日志消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void info(const QString& message, const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief 记录警告级别日志
     * @param message 日志消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void warning(const QString& message, const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief 记录错误级别日志
     * @param message 日志消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void error(const QString& message, const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief 记录致命错误级别日志
     * @param message 日志消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void fatal(const QString& message, const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief 记录日志
     * @param level 日志级别
     * @param message 日志消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void log(LogLevel level, const QString& message, const char* file = nullptr, int line = 0, const char* function = nullptr);

    /**
     * @brief 获取日志级别的字符串表示
     * @param level 日志级别
     * @return 字符串表示
     */
    static QString levelToString(LogLevel level);

    /**
     * @brief 从字符串解析日志级别
     * @param levelStr 日志级别字符串
     * @return 日志级别
     */
    static LogLevel levelFromString(const QString& levelStr);

signals:
    /**
     * @brief 日志记录信号
     * @param level 日志级别
     * @param message 日志消息
     * @param timestamp 时间戳
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    void logRecorded(LogLevel level, const QString& message, const QDateTime& timestamp, 
                    const QString& file, int line, const QString& function);

protected:
    // 析构函数
    ~Logger();

private:
    // 私有构造函数，防止外部创建实例
    Logger();
    
    // 禁用拷贝构造和赋值操作
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 格式化日志消息
    QString formatLogMessage(LogLevel level, const QString& message, const QDateTime& timestamp, 
                            const QString& file, int line, const QString& function) const;

    // 写入日志到控制台
    void writeToConsole(const QString& formattedMessage, LogLevel level);

    // 写入日志到文件
    void writeToFile(const QString& formattedMessage);

    // 日志级别
    LogLevel m_logLevel;
    
    // 是否启用控制台输出
    bool m_consoleEnabled;
    
    // 是否启用文件输出
    bool m_fileEnabled;
    
    // 日志文件路径
    QString m_logFilePath;
    
    // 日志文件
    QFile m_logFile;
    
    // 文本流
    QTextStream m_textStream;
    
    // 互斥锁，保证线程安全
    mutable QMutex m_mutex;
    
    // 单例实例
    static std::unique_ptr<Logger, LoggerDeleter> s_instance;

    // 日志级别到颜色的映射
    QMap<LogLevel, QString> m_levelColors;
};

/**
 * @brief Logger的删除器类
 */
class LoggerDeleter
{
public:
    void operator()(Logger* logger) const
    {
        delete logger;
    }
};

// 便捷宏定义
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL(message) Logger::instance().fatal(message, __FILE__, __LINE__, __FUNCTION__)

#endif // LOGGER_H 