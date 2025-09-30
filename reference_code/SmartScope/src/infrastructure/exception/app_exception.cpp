#include "infrastructure/exception/app_exception.h"
#include "infrastructure/logging/logger.h"

// 构造函数
AppException::AppException(const QString& message, const QString& file, int line, const QString& function)
    : m_message(message), m_file(file), m_line(line), m_function(function), m_timestamp(QDateTime::currentDateTime())
{
    Logger::instance().error(QString("[AppException] %1").arg(message));
}

// 析构函数
AppException::~AppException() noexcept
{
}

// 获取异常消息
const char* AppException::what() const noexcept
{
    try
    {
        m_whatCache = getFormattedMessage().toStdString();
        return m_whatCache.c_str();
    }
    catch (...)
    {
        return "Error formatting exception message";
    }
}

// 获取异常消息（QString类型）
QString AppException::getMessage() const
{
    return m_message;
}

// 获取源文件名
QString AppException::getFile() const
{
    return m_file;
}

// 获取行号
int AppException::getLine() const
{
    return m_line;
}

// 获取函数名
QString AppException::getFunction() const
{
    return m_function;
}

// 获取时间戳
QDateTime AppException::getTimestamp() const
{
    return m_timestamp;
}

// 获取格式化的异常信息
QString AppException::getFormattedMessage() const
{
    QString formattedMessage = QString("%1 [%2] %3")
        .arg(m_timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(getTypeName())
        .arg(m_message);
    
    if (!m_file.isEmpty() && m_line > 0 && !m_function.isEmpty())
    {
        formattedMessage += QString(" [%1:%2 %3]")
            .arg(m_file)
            .arg(m_line)
            .arg(m_function);
    }
    
    return formattedMessage;
}

// 获取异常类型名称
QString AppException::getTypeName() const
{
    return "AppException";
} 