#include "infrastructure/exception/network_exception.h"
#include "infrastructure/logging/logger.h"

// NetworkException 实现
NetworkException::NetworkException(const QString& message, const QString& file, int line, const QString& function)
    : AppException(message, file, line, function)
{
    Logger::instance().error(QString("[NetworkException] %1").arg(message));
}

NetworkException::~NetworkException() noexcept
{
}

QString NetworkException::getTypeName() const
{
    return "NetworkException";
}

// NetworkConnectionException 实现
NetworkConnectionException::NetworkConnectionException(const QString& host, int port, int errorCode, 
                                                     const QString& file, int line, const QString& function)
    : NetworkException(QString("无法连接到主机 '%1:%2'，错误码：%3").arg(host).arg(port).arg(errorCode), 
                      file, line, function),
      m_host(host), m_port(port), m_errorCode(errorCode)
{
    Logger::instance().error(QString("[NetworkConnectionException] 无法连接到主机 '%1:%2'，错误码：%3").arg(
                            host).arg(port).arg(errorCode));
}

NetworkConnectionException::~NetworkConnectionException() noexcept
{
}

QString NetworkConnectionException::getTypeName() const
{
    return "NetworkConnectionException";
}

QString NetworkConnectionException::getHost() const
{
    return m_host;
}

int NetworkConnectionException::getPort() const
{
    return m_port;
}

int NetworkConnectionException::getErrorCode() const
{
    return m_errorCode;
}

// NetworkTimeoutException 实现
NetworkTimeoutException::NetworkTimeoutException(const QString& operation, const QString& url, int timeout, 
                                               const QString& file, int line, const QString& function)
    : NetworkException(QString("%1请求 '%2' 超时，超时时间：%3毫秒").arg(operation, url).arg(timeout), 
                      file, line, function),
      m_operation(operation), m_url(url), m_timeout(timeout)
{
    Logger::instance().error(QString("[NetworkTimeoutException] %1请求 '%2' 超时，超时时间：%3毫秒").arg(
                            operation, url).arg(timeout));
}

NetworkTimeoutException::~NetworkTimeoutException() noexcept
{
}

QString NetworkTimeoutException::getTypeName() const
{
    return "NetworkTimeoutException";
}

QString NetworkTimeoutException::getOperation() const
{
    return m_operation;
}

QString NetworkTimeoutException::getUrl() const
{
    return m_url;
}

int NetworkTimeoutException::getTimeout() const
{
    return m_timeout;
}

// HttpException 实现
HttpException::HttpException(const QString& url, const QString& method, int statusCode, const QString& response, 
                           const QString& file, int line, const QString& function)
    : NetworkException(QString("HTTP %1 请求 '%2' 失败，状态码：%3").arg(method, url).arg(statusCode), 
                      file, line, function),
      m_url(url), m_method(method), m_statusCode(statusCode), m_response(response)
{
    Logger::instance().error(QString("[HttpException] HTTP %1 请求 '%2' 失败，状态码：%3").arg(
                            method, url).arg(statusCode));
    Logger::instance().debug(QString("[HttpException] 响应内容：%1").arg(response));
}

HttpException::~HttpException() noexcept
{
}

QString HttpException::getTypeName() const
{
    return "HttpException";
}

QString HttpException::getUrl() const
{
    return m_url;
}

QString HttpException::getMethod() const
{
    return m_method;
}

int HttpException::getStatusCode() const
{
    return m_statusCode;
}

QString HttpException::getResponse() const
{
    return m_response;
} 