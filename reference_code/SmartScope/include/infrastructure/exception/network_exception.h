#ifndef NETWORK_EXCEPTION_H
#define NETWORK_EXCEPTION_H

#include "infrastructure/exception/app_exception.h"

/**
 * @brief 网络异常类
 * 
 * 用于处理网络操作相关的异常
 */
class NetworkException : public AppException
{
public:
    /**
     * @brief 构造函数
     * @param message 异常消息
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    NetworkException(const QString& message, const QString& file = QString(), 
                    int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~NetworkException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
};

// 便捷宏定义
#define THROW_NETWORK_EXCEPTION(message) throw NetworkException(message, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 网络连接异常类
 * 
 * 当网络连接失败时抛出
 */
class NetworkConnectionException : public NetworkException
{
public:
    /**
     * @brief 构造函数
     * @param host 主机地址
     * @param port 端口号
     * @param errorCode 错误码
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    NetworkConnectionException(const QString& host, int port, int errorCode, 
                              const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~NetworkConnectionException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取主机地址
     * @return 主机地址
     */
    QString getHost() const;
    
    /**
     * @brief 获取端口号
     * @return 端口号
     */
    int getPort() const;
    
    /**
     * @brief 获取错误码
     * @return 错误码
     */
    int getErrorCode() const;

private:
    // 主机地址
    QString m_host;
    
    // 端口号
    int m_port;
    
    // 错误码
    int m_errorCode;
};

// 便捷宏定义
#define THROW_NETWORK_CONNECTION_EXCEPTION(host, port, errorCode) throw NetworkConnectionException(host, port, errorCode, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief 网络超时异常类
 * 
 * 当网络操作超时时抛出
 */
class NetworkTimeoutException : public NetworkException
{
public:
    /**
     * @brief 构造函数
     * @param operation 操作类型
     * @param url 请求URL
     * @param timeout 超时时间（毫秒）
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    NetworkTimeoutException(const QString& operation, const QString& url, int timeout, 
                           const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~NetworkTimeoutException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取操作类型
     * @return 操作类型
     */
    QString getOperation() const;
    
    /**
     * @brief 获取请求URL
     * @return 请求URL
     */
    QString getUrl() const;
    
    /**
     * @brief 获取超时时间
     * @return 超时时间（毫秒）
     */
    int getTimeout() const;

private:
    // 操作类型
    QString m_operation;
    
    // 请求URL
    QString m_url;
    
    // 超时时间
    int m_timeout;
};

// 便捷宏定义
#define THROW_NETWORK_TIMEOUT_EXCEPTION(operation, url, timeout) throw NetworkTimeoutException(operation, url, timeout, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief HTTP异常类
 * 
 * 当HTTP请求失败时抛出
 */
class HttpException : public NetworkException
{
public:
    /**
     * @brief 构造函数
     * @param url 请求URL
     * @param method HTTP方法
     * @param statusCode HTTP状态码
     * @param response 响应内容
     * @param file 源文件名
     * @param line 行号
     * @param function 函数名
     */
    HttpException(const QString& url, const QString& method, int statusCode, const QString& response, 
                 const QString& file = QString(), int line = 0, const QString& function = QString());
    
    /**
     * @brief 析构函数
     */
    virtual ~HttpException() noexcept;
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override;
    
    /**
     * @brief 获取请求URL
     * @return 请求URL
     */
    QString getUrl() const;
    
    /**
     * @brief 获取HTTP方法
     * @return HTTP方法
     */
    QString getMethod() const;
    
    /**
     * @brief 获取HTTP状态码
     * @return HTTP状态码
     */
    int getStatusCode() const;
    
    /**
     * @brief 获取响应内容
     * @return 响应内容
     */
    QString getResponse() const;

private:
    // 请求URL
    QString m_url;
    
    // HTTP方法
    QString m_method;
    
    // HTTP状态码
    int m_statusCode;
    
    // 响应内容
    QString m_response;
};

// 便捷宏定义
#define THROW_HTTP_EXCEPTION(url, method, statusCode, response) throw HttpException(url, method, statusCode, response, __FILE__, __LINE__, __FUNCTION__)

#endif // NETWORK_EXCEPTION_H 