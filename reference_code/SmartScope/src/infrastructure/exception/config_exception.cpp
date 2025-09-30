#include "infrastructure/exception/config_exception.h"
#include "infrastructure/logging/logger.h"

// ConfigException 实现
ConfigException::ConfigException(const QString& message, const QString& file, int line, const QString& function)
    : AppException(message, file, line, function)
{
    Logger::instance().error(QString("[ConfigException] %1").arg(message));
}

ConfigException::~ConfigException() noexcept
{
}

QString ConfigException::getTypeName() const
{
    return "ConfigException";
}

// ConfigKeyNotFoundException 实现
ConfigKeyNotFoundException::ConfigKeyNotFoundException(const QString& key, const QString& file, int line, const QString& function)
    : ConfigException(QString("配置键 '%1' 不存在").arg(key), file, line, function), m_key(key)
{
    Logger::instance().error(QString("[ConfigKeyNotFoundException] 配置键 '%1' 不存在").arg(key));
}

ConfigKeyNotFoundException::~ConfigKeyNotFoundException() noexcept
{
}

QString ConfigKeyNotFoundException::getTypeName() const
{
    return "ConfigKeyNotFoundException";
}

QString ConfigKeyNotFoundException::getKey() const
{
    return m_key;
}

// ConfigTypeException 实现
ConfigTypeException::ConfigTypeException(const QString& key, const QString& expectedType, const QString& actualType, 
                                       const QString& file, int line, const QString& function)
    : ConfigException(QString("配置键 '%1' 的类型错误，期望 '%2'，实际为 '%3'").arg(key, expectedType, actualType), 
                     file, line, function),
      m_key(key), m_expectedType(expectedType), m_actualType(actualType)
{
    Logger::instance().error(QString("[ConfigTypeException] 配置键 '%1' 的类型错误，期望 '%2'，实际为 '%3'").arg(
                            key, expectedType, actualType));
}

ConfigTypeException::~ConfigTypeException() noexcept
{
}

QString ConfigTypeException::getTypeName() const
{
    return "ConfigTypeException";
}

QString ConfigTypeException::getKey() const
{
    return m_key;
}

QString ConfigTypeException::getExpectedType() const
{
    return m_expectedType;
}

QString ConfigTypeException::getActualType() const
{
    return m_actualType;
}

// ConfigValidationException 实现
ConfigValidationException::ConfigValidationException(const QString& key, const QString& value, const QString& reason, 
                                                   const QString& file, int line, const QString& function)
    : ConfigException(QString("配置键 '%1' 的值 '%2' 验证失败：%3").arg(key, value, reason), 
                     file, line, function),
      m_key(key), m_value(value), m_reason(reason)
{
    Logger::instance().error(QString("[ConfigValidationException] 配置键 '%1' 的值 '%2' 验证失败：%3").arg(
                            key, value, reason));
}

ConfigValidationException::~ConfigValidationException() noexcept
{
}

QString ConfigValidationException::getTypeName() const
{
    return "ConfigValidationException";
}

QString ConfigValidationException::getKey() const
{
    return m_key;
}

QString ConfigValidationException::getValue() const
{
    return m_value;
}

QString ConfigValidationException::getReason() const
{
    return m_reason;
} 