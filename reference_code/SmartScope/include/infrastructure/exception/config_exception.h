#ifndef CONFIG_EXCEPTION_H
#define CONFIG_EXCEPTION_H

#include "infrastructure/exception/app_exception.h"

/**
 * @brief 配置异常类
 * 
 * 用于处理配置相关的异常
 */
class ConfigException : public AppException
{
public:
    /**
     * @brief 构造函数
     * @param message 异常消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    ConfigException(const QString& message, const QString& file = QString(), 
                   int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~ConfigException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
};

// 便捷宏定义
#define THROW_CONFIG_EXCEPTION(message) throw ConfigException(message, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 配置键不存在异常类
 * 
 * 当请求的配置键不存在时抛出
 */
class ConfigKeyNotFoundException : public ConfigException
{
public:
    /**
     * @brief 构造函数
     * @param key 配置键
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    ConfigKeyNotFoundException(const QString& key, const QString& file = QString(), 
                              int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~ConfigKeyNotFoundException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取配置键
     * @return 配置键
     */
    QString getKey() const;

private:
    // 配置键
    QString m_key;
};

// 便捷宏定义
#define THROW_CONFIG_KEY_NOT_FOUND_EXCEPTION(key) throw ConfigKeyNotFoundException(key, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 配置类型错误异常类
 * 
 * 当配置值的类型与预期不符时抛出
 */
class ConfigTypeException : public ConfigException
{
public:
    /**
     * @brief 构造函数
     * @param key 配置键
     * @param expectedType 预期类型
     * @param actualType 实际类型
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    ConfigTypeException(const QString& key, const QString& expectedType, const QString& actualType, 
                       const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~ConfigTypeException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取配置键
     * @return 配置键
     */
    QString getKey() const;
    
    /**
     * @brief 获取预期类型
     * @return 预期类型
     */
    QString getExpectedType() const;
    
    /**
     * @brief 获取实际类型
     * @return 实际类型
     */
    QString getActualType() const;

private:
    // 配置键
    QString m_key;
    
    // 预期类型
    QString m_expectedType;
    
    // 实际类型
    QString m_actualType;
};

// 便捷宏定义
#define THROW_CONFIG_TYPE_EXCEPTION(key, expectedType, actualType) throw ConfigTypeException(key, expectedType, actualType, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 配置验证异常类
 * 
 * 当配置验证失败时抛出
 */
class ConfigValidationException : public ConfigException
{
public:
    /**
     * @brief 构造函数
     * @param key 配置键
     * @param value 配置值
     * @param reason 验证失败原因
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    ConfigValidationException(const QString& key, const QString& value, const QString& reason, 
                             const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~ConfigValidationException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取配置键
     * @return 配置键
     */
    QString getKey() const;
    
    /**
     * @brief 获取配置值
     * @return 配置值
     */
    QString getValue() const;
    
    /**
     * @brief 获取验证失败原因
     * @return 验证失败原因
     */
    QString getReason() const;

private:
    // 配置键
    QString m_key;
    
    // 配置值
    QString m_value;
    
    // 验证失败原因
    QString m_reason;
};

// 便捷宏定义
#define THROW_CONFIG_VALIDATION_EXCEPTION(key, value, reason) throw ConfigValidationException(key, value, reason, __FILE__, __LINE__, __FUNCTION__)

#endif // CONFIG_EXCEPTION_H 