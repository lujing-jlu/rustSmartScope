#ifndef BUSINESS_EXCEPTION_H
#define BUSINESS_EXCEPTION_H

#include "infrastructure/exception/app_exception.h"

/**
 * @brief 业务异常类
 * 
 * 用于处理业务逻辑相关的异常
 */
class BusinessException : public AppException
{
public:
    /**
     * @brief 构造函数
     * @param message 异常消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    BusinessException(const QString& message, const QString& file = QString(), 
                     int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~BusinessException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
};

// 便捷宏定义
#define THROW_BUSINESS_EXCEPTION(message) throw BusinessException(message, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 数据验证异常类
 * 
 * 当数据验证失败时抛出
 */
class ValidationException : public BusinessException
{
public:
    /**
     * @brief 构造函数
     * @param field 字段名
     * @param value 字段值
     * @param reason 验证失败原因
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    ValidationException(const QString& field, const QString& value, const QString& reason, 
                       const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~ValidationException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取字段名
     * @return 字段名
     */
    QString getField() const;
    
    /**
     * @brief 获取字段值
     * @return 字段值
     */
    QString getValue() const;
    
    /**
     * @brief 获取验证失败原因
     * @return 验证失败原因
     */
    QString getReason() const;

private:
    // 字段名
    QString m_field;
    
    // 字段值
    QString m_value;
    
    // 验证失败原因
    QString m_reason;
};

// 便捷宏定义
#define THROW_VALIDATION_EXCEPTION(field, value, reason) throw ValidationException(field, value, reason, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 数据不存在异常类
 * 
 * 当请求的数据不存在时抛出
 */
class DataNotFoundException : public BusinessException
{
public:
    /**
     * @brief 构造函数
     * @param entityType 实体类型
     * @param id 实体ID
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    DataNotFoundException(const QString& entityType, const QString& id, 
                         const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~DataNotFoundException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取实体类型
     * @return 实体类型
     */
    QString getEntityType() const;
    
    /**
     * @brief 获取实体ID
     * @return 实体ID
     */
    QString getId() const;

private:
    // 实体类型
    QString m_entityType;
    
    // 实体ID
    QString m_id;
};

// 便捷宏定义
#define THROW_DATA_NOT_FOUND_EXCEPTION(entityType, id) throw DataNotFoundException(entityType, id, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 数据重复异常类
 * 
 * 当数据已存在时抛出
 */
class DuplicateDataException : public BusinessException
{
public:
    /**
     * @brief 构造函数
     * @param entityType 实体类型
     * @param field 字段名
     * @param value 字段值
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    DuplicateDataException(const QString& entityType, const QString& field, const QString& value, 
                          const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~DuplicateDataException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取实体类型
     * @return 实体类型
     */
    QString getEntityType() const;
    
    /**
     * @brief 获取字段名
     * @return 字段名
     */
    QString getField() const;
    
    /**
     * @brief 获取字段值
     * @return 字段值
     */
    QString getValue() const;

private:
    // 实体类型
    QString m_entityType;
    
    // 字段名
    QString m_field;
    
    // 字段值
    QString m_value;
};

// 便捷宏定义
#define THROW_DUPLICATE_DATA_EXCEPTION(entityType, field, value) throw DuplicateDataException(entityType, field, value, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 业务操作异常类
 * 
 * 当业务操作失败时抛出
 */
class BusinessOperationException : public BusinessException
{
public:
    /**
     * @brief 构造函数
     * @param operation 操作名称
     * @param reason 失败原因
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    BusinessOperationException(const QString& operation, const QString& reason, 
                              const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~BusinessOperationException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取操作名称
     * @return 操作名称
     */
    QString getOperation() const;
    
    /**
     * @brief 获取失败原因
     * @return 失败原因
     */
    QString getReason() const;

private:
    // 操作名称
    QString m_operation;
    
    // 失败原因
    QString m_reason;
};

// 便捷宏定义
#define THROW_BUSINESS_OPERATION_EXCEPTION(operation, reason) throw BusinessOperationException(operation, reason, __FILE__, __LINE__, __FUNCTION__)

#endif // BUSINESS_EXCEPTION_H 