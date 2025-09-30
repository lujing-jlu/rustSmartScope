#include "infrastructure/exception/business_exception.h"
#include "infrastructure/logging/logger.h"

// BusinessException 实现
BusinessException::BusinessException(const QString& message, const QString& file, int line, const QString& function)
    : AppException(message, file, line, function)
{
    Logger::instance().error(QString("[BusinessException] %1").arg(message));
}

BusinessException::~BusinessException() noexcept
{
}

QString BusinessException::getTypeName() const
{
    return "BusinessException";
}

// ValidationException 实现
ValidationException::ValidationException(const QString& field, const QString& value, const QString& reason, 
                                       const QString& file, int line, const QString& function)
    : BusinessException(QString("字段 '%1' 的值 '%2' 验证失败：%3").arg(field, value, reason), 
                       file, line, function),
      m_field(field), m_value(value), m_reason(reason)
{
    Logger::instance().error(QString("[ValidationException] 字段 '%1' 的值 '%2' 验证失败：%3").arg(
                            field, value, reason));
}

ValidationException::~ValidationException() noexcept
{
}

QString ValidationException::getTypeName() const
{
    return "ValidationException";
}

QString ValidationException::getField() const
{
    return m_field;
}

QString ValidationException::getValue() const
{
    return m_value;
}

QString ValidationException::getReason() const
{
    return m_reason;
}

// DataNotFoundException 实现
DataNotFoundException::DataNotFoundException(const QString& entityType, const QString& id, 
                                           const QString& file, int line, const QString& function)
    : BusinessException(QString("实体 '%1' 的ID '%2' 不存在").arg(entityType, id), 
                       file, line, function),
      m_entityType(entityType), m_id(id)
{
    Logger::instance().error(QString("[DataNotFoundException] 实体 '%1' 的ID '%2' 不存在").arg(
                            entityType, id));
}

DataNotFoundException::~DataNotFoundException() noexcept
{
}

QString DataNotFoundException::getTypeName() const
{
    return "DataNotFoundException";
}

QString DataNotFoundException::getEntityType() const
{
    return m_entityType;
}

QString DataNotFoundException::getId() const
{
    return m_id;
}

// DuplicateDataException 实现
DuplicateDataException::DuplicateDataException(const QString& entityType, const QString& field, const QString& value, 
                                             const QString& file, int line, const QString& function)
    : BusinessException(QString("实体 '%1' 的字段 '%2' 的值 '%3' 已存在").arg(entityType, field, value), 
                       file, line, function),
      m_entityType(entityType), m_field(field), m_value(value)
{
    Logger::instance().error(QString("[DuplicateDataException] 实体 '%1' 的字段 '%2' 的值 '%3' 已存在").arg(
                            entityType, field, value));
}

DuplicateDataException::~DuplicateDataException() noexcept
{
}

QString DuplicateDataException::getTypeName() const
{
    return "DuplicateDataException";
}

QString DuplicateDataException::getEntityType() const
{
    return m_entityType;
}

QString DuplicateDataException::getField() const
{
    return m_field;
}

QString DuplicateDataException::getValue() const
{
    return m_value;
}

// BusinessOperationException 实现
BusinessOperationException::BusinessOperationException(const QString& operation, const QString& reason, 
                                                     const QString& file, int line, const QString& function)
    : BusinessException(QString("业务操作 '%1' 失败：%2").arg(operation, reason), 
                       file, line, function),
      m_operation(operation), m_reason(reason)
{
    Logger::instance().error(QString("[BusinessOperationException] 业务操作 '%1' 失败：%2").arg(
                            operation, reason));
}

BusinessOperationException::~BusinessOperationException() noexcept
{
}

QString BusinessOperationException::getTypeName() const
{
    return "BusinessOperationException";
}

QString BusinessOperationException::getOperation() const
{
    return m_operation;
}

QString BusinessOperationException::getReason() const
{
    return m_reason;
} 