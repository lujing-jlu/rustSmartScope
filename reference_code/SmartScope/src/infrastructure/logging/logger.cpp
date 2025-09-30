#include "infrastructure/logging/logger.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMutexLocker>
#include <QThread>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

// 初始化静态单例指针
std::unique_ptr<Logger, LoggerDeleter> Logger::s_instance = nullptr;

// 获取单例实例
Logger& Logger::instance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<Logger, LoggerDeleter>(new Logger());
    }
    return *s_instance;
}

// 构造函数
Logger::Logger() : QObject(nullptr),
    m_logLevel(LogLevel::INFO),
    m_consoleEnabled(true),
    m_fileEnabled(true)
{
    // 初始化日志级别颜色映射
    m_levelColors[LogLevel::DEBUG] = "\033[37m";    // 白色
    m_levelColors[LogLevel::INFO] = "\033[32m";     // 绿色
    m_levelColors[LogLevel::WARNING] = "\033[33m";  // 黄色
    m_levelColors[LogLevel::ERROR] = "\033[31m";    // 红色
    m_levelColors[LogLevel::FATAL] = "\033[35m";    // 紫色
}

// 析构函数
Logger::~Logger()
{
    // 关闭日志文件
    if (m_logFile.isOpen()) {
        m_textStream.flush();
        m_logFile.close();
    }
}

// 初始化日志系统
bool Logger::init(const QString& logFilePath, LogLevel logLevel, bool enableConsole, bool enableFile)
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "Logger::init - 开始初始化日志系统";
    qDebug() << "Logger::init - 日志文件路径:" << logFilePath;
    qDebug() << "Logger::init - 日志级别:" << levelToString(logLevel);
    qDebug() << "Logger::init - 启用控制台输出:" << enableConsole;
    qDebug() << "Logger::init - 启用文件输出:" << enableFile;
    
    m_logLevel = logLevel;
    m_consoleEnabled = enableConsole;
    m_fileEnabled = enableFile;
    
    // 如果不启用文件输出，直接返回成功
    if (!m_fileEnabled) {
        qDebug() << "Logger::init - 文件输出已禁用，初始化完成";
        return true;
    }
    
    // 直接设置日志文件路径，不使用异步操作
    bool success = setLogFilePath(logFilePath);
    if (!success) {
        qWarning() << "Logger::init - 设置日志文件路径失败，禁用文件输出";
        m_fileEnabled = false;
        return true;
    }
    
    qDebug() << "Logger::init - 初始化完成";
    return true;
}

// 设置日志级别
void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

// 获取当前日志级别
LogLevel Logger::getLogLevel() const
{
    QMutexLocker locker(&m_mutex);
    return m_logLevel;
}

// 设置是否启用控制台输出
void Logger::setConsoleEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_consoleEnabled = enable;
}

// 设置是否启用文件输出
void Logger::setFileEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果之前未启用文件输出，现在启用，需要打开日志文件
    if (!m_fileEnabled && enable && !m_logFilePath.isEmpty()) {
        if (m_logFile.isOpen()) {
            m_logFile.close();
        }
        
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "无法打开日志文件:" << m_logFilePath;
            return;
        }
        
        m_textStream.setDevice(&m_logFile);
    }
    // 如果之前启用文件输出，现在禁用，需要关闭日志文件
    else if (m_fileEnabled && !enable && m_logFile.isOpen()) {
        m_textStream.flush();
        m_logFile.close();
    }
    
    m_fileEnabled = enable;
}

// 设置日志文件路径
bool Logger::setLogFilePath(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    
    QString path = filePath;
    
    qDebug() << "Logger::setLogFilePath - 开始设置日志文件路径:" << path;
    
    // 如果未指定日志文件路径，使用默认路径
    if (path.isEmpty()) {
        qDebug() << "Logger::setLogFilePath - 未指定日志文件路径，使用默认路径";
        // 首先尝试应用程序目录下的logs目录
        path = QCoreApplication::applicationDirPath() + "/logs";
        QDir dir(path);
        if (!dir.exists()) {
            qDebug() << "Logger::setLogFilePath - 创建日志目录:" << path;
            if (!dir.mkpath(".")) {
                qWarning() << "Logger::setLogFilePath - 无法创建日志目录:" << path;
                return false;
            }
        }
        
        // 使用应用程序名称作为日志文件名
        QString appName = QCoreApplication::applicationName();
        if (appName.isEmpty()) {
            appName = "app";
        }
        
        path += "/" + appName + ".log";
        qDebug() << "Logger::setLogFilePath - 使用默认日志文件路径:" << path;
    }
    
    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        qDebug() << "Logger::setLogFilePath - 创建日志目录:" << dir.path();
        if (!dir.mkpath(".")) {
            qWarning() << "Logger::setLogFilePath - 无法创建日志目录:" << dir.path();
            return false;
        }
    }
    
    // 关闭之前的日志文件
    if (m_logFile.isOpen()) {
        qDebug() << "Logger::setLogFilePath - 关闭之前的日志文件";
        m_textStream.flush();
        m_logFile.close();
    }
    
    // 打开新的日志文件
    m_logFilePath = path;
    
    if (m_fileEnabled) {
        qDebug() << "Logger::setLogFilePath - 打开新的日志文件:" << m_logFilePath;
        m_logFile.setFileName(m_logFilePath);
        
        // 使用标准方式打开文件
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "Logger::setLogFilePath - 无法打开日志文件:" << m_logFilePath << ", 错误:" << m_logFile.errorString();
            return false;
        }
        
        m_textStream.setDevice(&m_logFile);
        qDebug() << "Logger::setLogFilePath - 日志文件打开成功";
    } else {
        qDebug() << "Logger::setLogFilePath - 文件输出已禁用，不打开日志文件";
    }
    
    return true;
}

// 获取日志文件路径
QString Logger::getLogFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_logFilePath;
}

// 记录调试级别日志
void Logger::debug(const QString& message, const char* file, int line, const char* function)
{
    log(LogLevel::DEBUG, message, file, line, function);
}

// 记录信息级别日志
void Logger::info(const QString& message, const char* file, int line, const char* function)
{
    log(LogLevel::INFO, message, file, line, function);
}

// 记录警告级别日志
void Logger::warning(const QString& message, const char* file, int line, const char* function)
{
    log(LogLevel::WARNING, message, file, line, function);
}

// 记录错误级别日志
void Logger::error(const QString& message, const char* file, int line, const char* function)
{
    log(LogLevel::ERROR, message, file, line, function);
}

// 记录致命错误级别日志
void Logger::fatal(const QString& message, const char* file, int line, const char* function)
{
    log(LogLevel::FATAL, message, file, line, function);
}

// 记录日志
void Logger::log(LogLevel level, const QString& message, const char* file, int line, const char* function)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果日志级别低于当前设置的级别，不记录
    if (level < m_logLevel) {
        return;
    }
    
    // 获取当前时间
    QDateTime timestamp = QDateTime::currentDateTime();
    
    // 获取文件名、行号和函数名
    QString fileName;
    if (file) {
        fileName = QString(file);
        // 只保留文件名，去掉路径
        int pos = fileName.lastIndexOf('/');
        if (pos >= 0) {
            fileName = fileName.mid(pos + 1);
        }
    }
    
    QString functionName;
    if (function) {
        functionName = QString(function);
    }
    
    // 格式化日志消息
    QString formattedMessage = formatLogMessage(level, message, timestamp, fileName, line, functionName);
    
    // 输出到控制台
    if (m_consoleEnabled) {
        writeToConsole(formattedMessage, level);
    }
    
    // 输出到文件
    if (m_fileEnabled) {
        try {
            // 直接写入文件，不使用QTextStream
            writeToFile(formattedMessage);
        } catch (const std::exception& e) {
            qWarning() << "Logger::log - 写入日志文件异常:" << e.what();
        } catch (...) {
            qWarning() << "Logger::log - 写入日志文件未知异常";
        }
    }
    
    // 发送日志记录信号
    emit logRecorded(level, message, timestamp, fileName, line, functionName);
}

// 格式化日志消息
QString Logger::formatLogMessage(LogLevel level, const QString& message, const QDateTime& timestamp, 
                                const QString& file, int line, const QString& function) const
{
    QString result;
    
    // 添加时间戳
    result += timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz ");
    
    // 添加日志级别
    result += "[" + levelToString(level) + "] ";
    
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
}

// 写入日志到控制台
void Logger::writeToConsole(const QString& formattedMessage, LogLevel level)
{
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

// 写入日志到文件
void Logger::writeToFile(const QString& formattedMessage)
{
    qDebug() << "Logger::writeToFile - 开始写入日志:" << formattedMessage.left(30) + "...";
    
    // 检查文件是否打开
    if (!m_logFile.isOpen()) {
        qWarning() << "Logger::writeToFile - 日志文件未打开";
        
        // 尝试重新打开文件
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "Logger::writeToFile - 无法重新打开日志文件:" << m_logFilePath << ", 错误:" << m_logFile.errorString();
            return;
        }
        
        m_textStream.setDevice(&m_logFile);
        qDebug() << "Logger::writeToFile - 日志文件重新打开成功";
    }
    
    // 使用QFile直接写入，而不是QTextStream
    QByteArray data = (formattedMessage + "\n").toUtf8();
    qint64 bytesWritten = m_logFile.write(data);
    
    if (bytesWritten == -1) {
        qWarning() << "Logger::writeToFile - 写入日志文件失败:" << m_logFile.errorString();
    } else if (bytesWritten != data.size()) {
        qWarning() << "Logger::writeToFile - 写入日志文件不完整:" << bytesWritten << "/" << data.size();
    } else {
        qDebug() << "Logger::writeToFile - 写入日志文件成功:" << bytesWritten << "字节";
    }
    
    // 立即刷新文件缓冲区
    m_logFile.flush();
}

// 获取日志级别的字符串表示
QString Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

// 从字符串解析日志级别
LogLevel Logger::levelFromString(const QString& levelStr)
{
    QString upperStr = levelStr.toUpper();
    
    if (upperStr == "DEBUG") {
        return LogLevel::DEBUG;
    } else if (upperStr == "INFO") {
        return LogLevel::INFO;
    } else if (upperStr == "WARNING") {
        return LogLevel::WARNING;
    } else if (upperStr == "ERROR") {
        return LogLevel::ERROR;
    } else if (upperStr == "FATAL") {
        return LogLevel::FATAL;
    } else {
        return LogLevel::INFO; // 默认为INFO级别
    }
} 