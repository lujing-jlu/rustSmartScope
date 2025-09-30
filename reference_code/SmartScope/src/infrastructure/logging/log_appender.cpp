#include "infrastructure/logging/log_appender.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>

// LogAppender 基类实现

// 构造函数
LogAppender::LogAppender(std::shared_ptr<LogFormatter> formatter)
    : m_logLevel(LogLevel::INFO)
{
    if (formatter) {
        m_formatter = formatter;
    } else {
        m_formatter = std::make_shared<LogFormatter>();
    }
}

// 析构函数
LogAppender::~LogAppender()
{
}

// 设置日志格式化器
void LogAppender::setFormatter(std::shared_ptr<LogFormatter> formatter)
{
    QMutexLocker locker(&m_mutex);
    if (formatter) {
        m_formatter = formatter;
    }
}

// 获取日志格式化器
std::shared_ptr<LogFormatter> LogAppender::getFormatter() const
{
    QMutexLocker locker(&m_mutex);
    return m_formatter;
}

// 设置日志级别
void LogAppender::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

// 获取日志级别
LogLevel LogAppender::getLogLevel() const
{
    QMutexLocker locker(&m_mutex);
    return m_logLevel;
}

// ConsoleAppender 实现

// 构造函数
ConsoleAppender::ConsoleAppender(std::shared_ptr<LogFormatter> formatter)
    : LogAppender(formatter)
{
    // 初始化日志级别颜色映射
    m_levelColors[LogLevel::DEBUG] = "\033[37m";    // 白色
    m_levelColors[LogLevel::INFO] = "\033[32m";     // 绿色
    m_levelColors[LogLevel::WARNING] = "\033[33m";  // 黄色
    m_levelColors[LogLevel::ERROR] = "\033[31m";    // 红色
    m_levelColors[LogLevel::FATAL] = "\033[35m";    // 紫色
}

// 析构函数
ConsoleAppender::~ConsoleAppender()
{
}

// 输出日志到控制台
void ConsoleAppender::append(LogLevel level, const QString& message, const QDateTime& timestamp, 
                           const QString& file, int line, const QString& function)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果日志级别低于当前设置的级别，不记录
    if (level < m_logLevel) {
        return;
    }
    
    // 格式化日志消息
    QString formattedMessage = m_formatter->format(level, message, timestamp, file, line, function);
    
    // 根据日志级别设置颜色
    QString colorCode = m_levelColors.value(level, "\033[0m");
    QString resetCode = "\033[0m";
    
    // 输出到控制台
    if (level >= LogLevel::ERROR) {
        qCritical().noquote() << colorCode << formattedMessage << resetCode;
    } else if (level == LogLevel::WARNING) {
        qWarning().noquote() << colorCode << formattedMessage << resetCode;
    } else {
        qDebug().noquote() << colorCode << formattedMessage << resetCode;
    }
}

// FileAppender 实现

// 构造函数
FileAppender::FileAppender(const QString& filePath, std::shared_ptr<LogFormatter> formatter)
    : LogAppender(formatter), m_filePath(filePath), m_autoFlush(true)
{
    // 设置日志文件路径
    setFilePath(filePath);
}

// 析构函数
FileAppender::~FileAppender()
{
    // 关闭日志文件
    if (m_file.isOpen()) {
        m_textStream.flush();
        m_file.close();
    }
}

// 输出日志到文件
void FileAppender::append(LogLevel level, const QString& message, const QDateTime& timestamp, 
                         const QString& file, int line, const QString& function)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果日志级别低于当前设置的级别，不记录
    if (level < m_logLevel) {
        return;
    }
    
    // 如果文件未打开，尝试打开
    if (!m_file.isOpen()) {
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "无法打开日志文件:" << m_filePath;
            return;
        }
        m_textStream.setDevice(&m_file);
    }
    
    // 格式化日志消息
    QString formattedMessage = m_formatter->format(level, message, timestamp, file, line, function);
    
    // 写入日志
    m_textStream << formattedMessage << Qt::endl;
    
    // 如果设置了自动刷新，刷新文件
    if (m_autoFlush) {
        m_textStream.flush();
    }
}

// 设置日志文件路径
bool FileAppender::setFilePath(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果文件路径为空，返回失败
    if (filePath.isEmpty()) {
        return false;
    }
    
    // 确保目录存在
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "无法创建日志目录:" << dir.path();
            return false;
        }
    }
    
    // 关闭之前的日志文件
    if (m_file.isOpen()) {
        m_textStream.flush();
        m_file.close();
    }
    
    // 设置新的日志文件路径
    m_filePath = filePath;
    m_file.setFileName(m_filePath);
    
    return true;
}

// 获取日志文件路径
QString FileAppender::getFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_filePath;
}

// 设置是否自动刷新
void FileAppender::setAutoFlush(bool autoFlush)
{
    QMutexLocker locker(&m_mutex);
    m_autoFlush = autoFlush;
}

// 获取是否自动刷新
bool FileAppender::getAutoFlush() const
{
    QMutexLocker locker(&m_mutex);
    return m_autoFlush;
}

// 刷新日志文件
void FileAppender::flush()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_textStream.flush();
    }
} 