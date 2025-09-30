#ifndef APP_EXCEPTION_H
#define APP_EXCEPTION_H

#include <QString>
#include <QDateTime>
#include <exception>

/**
 * @brief 应用程序异常基类
 * 
 * 所有应用程序异常的基类，提供统一的异常处理机制
 */
class AppException : public std::exception
{
public:
    /**
     * @brief 构造函数
     * @param message 异常消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    AppException(const QString& message, const QString& file = QString(), 
                int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~AppException() noexcept;
    
    /**
     * @brief 获取异常消息
     * @return 异常消息
     */
    virtual const char* what() const noexcept override;
    
    /**
     * @brief 获取异常消息（QString类型）
     * @return 异常消息
     */
    virtual QString getMessage() const;
    
    /**
     * @brief 获取源文件名
     * @return 源文件名
     */
    QString getFile() const;
    
    /**
     * @brief 获取行号
     * @return 行号
     */
    int getLine() const;
    
    /**
     * @brief 获取函数名
     * @return 函数名
     */
    QString getFunction() const;
    
    /**
     * @brief 获取时间戳
     * @return 时间戳
     */
    QDateTime getTimestamp() const;
    
    /**
     * @brief 获取格式化的异常信息
     * @return 格式化的异常信息
     */
    QString getFormattedMessage() const;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const;

protected:
    // 异常消息
    QString m_message;
    
    // 源文件名
    QString m_file;
    
    // 行号
    int m_line;
    
    // 函数名
    QString m_function;
    
    // 时间戳
    QDateTime m_timestamp;
    
    // 缓存的what()返回值
    mutable std::string m_whatCache;
};

// 便捷宏定义
#define THROW_APP_EXCEPTION(message) throw AppException(message, __FILE__, __LINE__, __FUNCTION__)

#endif // APP_EXCEPTION_H 