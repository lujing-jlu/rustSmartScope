#ifndef CAMERA_EXCEPTION_H
#define CAMERA_EXCEPTION_H

#include "infrastructure/exception/app_exception.h"
#include <QString>

namespace SmartScope {
namespace Infrastructure {

/**
 * @brief 相机异常类，用于表示相机操作中的异常情况
 */
class CameraException : public AppException
{
public:
    /**
     * @brief 构造函数
     * @param message 异常消息
     * @param file 异常发生的文件名
     * @param line 异常发生的行号
     * @param function 异常发生的函数名
     */
    CameraException(const QString& message, const QString& file = QString(), 
                    int line = 0, const QString& function = QString())
        : AppException(message, file, line, function)
    {
    }
    
    /**
     * @brief 析构函数
     */
    virtual ~CameraException() throw()
    {
    }
    
    /**
     * @brief 获取异常类型名称
     * @return 异常类型名称
     */
    virtual QString getTypeName() const override
    {
        return "CameraException";
    }
};

// 便捷宏定义，用于抛出相机异常
#define THROW_CAMERA_EXCEPTION(message) throw CameraException(message, __FILE__, __LINE__, __FUNCTION__)

} // namespace Infrastructure
} // namespace SmartScope

#endif // CAMERA_EXCEPTION_H 