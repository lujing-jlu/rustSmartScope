#ifndef FILE_EXCEPTION_H
#define FILE_EXCEPTION_H

#include "infrastructure/exception/app_exception.h"

/**
 * @brief 文件异常类
 * 
 * 用于处理文件操作相关的异常
 */
class FileException : public AppException
{
public:
    /**
     * @brief 构造函数
     * @param message 异常消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    FileException(const QString& message, const QString& file = QString(), 
                 int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~FileException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
};

// 便捷宏定义
#define THROW_FILE_EXCEPTION(message) throw FileException(message, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 文件不存在异常类
 * 
 * 当请求的文件不存在时抛出
 */
class FileNotFoundException : public FileException
{
public:
    /**
     * @brief 构造函数
     * @param filePath 文件路径
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    FileNotFoundException(const QString& filePath, const QString& file = QString(), 
                         int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~FileNotFoundException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取文件路径
     * @return 文件路径
     */
    QString getFilePath() const;

private:
    // 文件路径
    QString m_filePath;
};

// 便捷宏定义
#define THROW_FILE_NOT_FOUND_EXCEPTION(filePath) throw FileNotFoundException(filePath, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 文件访问权限异常类
 * 
 * 当没有足够的权限访问文件时抛出
 */
class FileAccessException : public FileException
{
public:
    /**
     * @brief 构造函数
     * @param filePath 文件路径
     * @param operation 操作类型（读/写/执行）
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    FileAccessException(const QString& filePath, const QString& operation, 
                       const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~FileAccessException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取文件路径
     * @return 文件路径
     */
    QString getFilePath() const;
    
    /**
     * @brief 获取操作类型
     * @return 操作类型
     */
    QString getOperation() const;

private:
    // 文件路径
    QString m_filePath;
    
    // 操作类型
    QString m_operation;
};

// 便捷宏定义
#define THROW_FILE_ACCESS_EXCEPTION(filePath, operation) throw FileAccessException(filePath, operation, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 文件格式异常类
 * 
 * 当文件格式不正确时抛出
 */
class FileFormatException : public FileException
{
public:
    /**
     * @brief 构造函数
     * @param filePath 文件路径
     * @param expectedFormat 预期格式
     * @param reason 格式错误原因
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    FileFormatException(const QString& filePath, const QString& expectedFormat, const QString& reason, 
                       const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~FileFormatException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取文件路径
     * @return 文件路径
     */
    QString getFilePath() const;
    
    /**
     * @brief 获取预期格式
     * @return 预期格式
     */
    QString getExpectedFormat() const;
    
    /**
     * @brief 获取格式错误原因
     * @return 格式错误原因
     */
    QString getReason() const;

private:
    // 文件路径
    QString m_filePath;
    
    // 预期格式
    QString m_expectedFormat;
    
    // 格式错误原因
    QString m_reason;
};

// 便捷宏定义
#define THROW_FILE_FORMAT_EXCEPTION(filePath, expectedFormat, reason) throw FileFormatException(filePath, expectedFormat, reason, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 文件IO异常类
 * 
 * 当文件IO操作失败时抛出
 */
class FileIOException : public FileException
{
public:
    /**
     * @brief 构造函数
     * @param filePath 文件路径
     * @param operation IO操作类型
     * @param errorCode 错误码
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    FileIOException(const QString& filePath, const QString& operation, int errorCode, 
                   const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~FileIOException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取文件路径
     * @return 文件路径
     */
    QString getFilePath() const;
    
    /**
     * @brief 获取IO操作类型
     * @return IO操作类型
     */
    QString getOperation() const;
    
    /**
     * @brief 获取错误码
     * @return 错误码
     */
    int getErrorCode() const;

private:
    // 文件路径
    QString m_filePath;
    
    // IO操作类型
    QString m_operation;
    
    // 错误码
    int m_errorCode;
};

// 便捷宏定义
#define THROW_FILE_IO_EXCEPTION(filePath, operation, errorCode) throw FileIOException(filePath, operation, errorCode, __FILE__, __LINE__, __FUNCTION__)

#endif // FILE_EXCEPTION_H 