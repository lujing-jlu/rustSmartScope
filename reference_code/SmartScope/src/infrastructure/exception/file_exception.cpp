#include "infrastructure/exception/file_exception.h"
#include "infrastructure/logging/logger.h"

// FileException 实现
FileException::FileException(const QString& message, const QString& file, int line, const QString& function)
    : AppException(message, file, line, function)
{
    Logger::instance().error(QString("[FileException] %1").arg(message));
}

FileException::~FileException() noexcept
{
}

QString FileException::getTypeName() const
{
    return "FileException";
}

// FileNotFoundException 实现
FileNotFoundException::FileNotFoundException(const QString& filePath, const QString& file, int line, const QString& function)
    : FileException(QString("文件 '%1' 不存在").arg(filePath), file, line, function), m_filePath(filePath)
{
    Logger::instance().error(QString("[FileNotFoundException] 文件 '%1' 不存在").arg(filePath));
}

FileNotFoundException::~FileNotFoundException() noexcept
{
}

QString FileNotFoundException::getTypeName() const
{
    return "FileNotFoundException";
}

QString FileNotFoundException::getFilePath() const
{
    return m_filePath;
}

// FileAccessException 实现
FileAccessException::FileAccessException(const QString& filePath, const QString& operation, 
                                       const QString& file, int line, const QString& function)
    : FileException(QString("无法%1文件 '%2'，权限不足").arg(operation, filePath), 
                   file, line, function),
      m_filePath(filePath), m_operation(operation)
{
    Logger::instance().error(QString("[FileAccessException] 无法%1文件 '%2'，权限不足").arg(operation, filePath));
}

FileAccessException::~FileAccessException() noexcept
{
}

QString FileAccessException::getTypeName() const
{
    return "FileAccessException";
}

QString FileAccessException::getFilePath() const
{
    return m_filePath;
}

QString FileAccessException::getOperation() const
{
    return m_operation;
}

// FileFormatException 实现
FileFormatException::FileFormatException(const QString& filePath, const QString& expectedFormat, const QString& reason, 
                                       const QString& file, int line, const QString& function)
    : FileException(QString("文件 '%1' 格式错误，期望 '%2'，原因：%3").arg(filePath, expectedFormat, reason), 
                   file, line, function),
      m_filePath(filePath), m_expectedFormat(expectedFormat), m_reason(reason)
{
    Logger::instance().error(QString("[FileFormatException] 文件 '%1' 格式错误，期望 '%2'，原因：%3").arg(
                            filePath, expectedFormat, reason));
}

FileFormatException::~FileFormatException() noexcept
{
}

QString FileFormatException::getTypeName() const
{
    return "FileFormatException";
}

QString FileFormatException::getFilePath() const
{
    return m_filePath;
}

QString FileFormatException::getExpectedFormat() const
{
    return m_expectedFormat;
}

QString FileFormatException::getReason() const
{
    return m_reason;
}

// FileIOException 实现
FileIOException::FileIOException(const QString& filePath, const QString& operation, int errorCode, 
                               const QString& file, int line, const QString& function)
    : FileException(QString("文件 '%1' %2操作失败，错误码：%3").arg(filePath, operation).arg(errorCode), 
                   file, line, function),
      m_filePath(filePath), m_operation(operation), m_errorCode(errorCode)
{
    Logger::instance().error(QString("[FileIOException] 文件 '%1' %2操作失败，错误码：%3").arg(
                            filePath, operation).arg(errorCode));
}

FileIOException::~FileIOException() noexcept
{
}

QString FileIOException::getTypeName() const
{
    return "FileIOException";
}

QString FileIOException::getFilePath() const
{
    return m_filePath;
}

QString FileIOException::getOperation() const
{
    return m_operation;
}

int FileIOException::getErrorCode() const
{
    return m_errorCode;
} 