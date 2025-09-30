#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include <QObject>
#include <QString>
#include <functional>
#include "infrastructure/exception/app_exception.h"

/**
 * @brief 异常处理器类
 * 
 * 提供统一的异常处理机制，支持异常捕获、日志记录和用户提示
 */
class ExceptionHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 异常处理器实例
     */
    static ExceptionHandler& instance();

    /**
     * @brief 处理可能抛出异常的代码
     * @param func 可能抛出异常的函数
     * @param showMessageBox 是否显示消息框
     * @return 是否成功执行
     */
    bool handle(const std::function<void()>& func, bool showMessageBox = true);

    /**
     * @brief 处理可能抛出异常的代码，并返回结果
     * @param func 可能抛出异常的函数
     * @param defaultValue 默认返回值
     * @param showMessageBox 是否显示消息框
     * @return 函数执行结果或默认值
     */
    template<typename T>
    T handleWithReturn(const std::function<T()>& func, const T& defaultValue = T(), bool showMessageBox = true)
    {
        try
        {
            return func();
        }
        catch (const AppException& e)
        {
            handleAppException(e, showMessageBox);
        }
        catch (const std::exception& e)
        {
            handleStdException(e, showMessageBox);
        }
        catch (...)
        {
            handleUnknownException(showMessageBox);
        }
        return defaultValue;
    }

signals:
    /**
     * @brief 异常发生信号
     * @param exception 应用异常
     */
    void exceptionOccurred(const AppException& exception);

private:
    // 私有构造函数，防止外部创建实例
    ExceptionHandler();
    ~ExceptionHandler();
    
    // 禁用拷贝构造和赋值操作
    ExceptionHandler(const ExceptionHandler&) = delete;
    ExceptionHandler& operator=(const ExceptionHandler&) = delete;

    /**
     * @brief 处理应用异常
     * @param e 应用异常
     * @param showMessageBox 是否显示消息框
     */
    void handleAppException(const AppException& e, bool showMessageBox);

    /**
     * @brief 处理标准异常
     * @param e 标准异常
     * @param showMessageBox 是否显示消息框
     */
    void handleStdException(const std::exception& e, bool showMessageBox);

    /**
     * @brief 处理未知异常
     * @param showMessageBox 是否显示消息框
     */
    void handleUnknownException(bool showMessageBox);

    /**
     * @brief 显示异常消息框
     * @param title 标题
     * @param message 消息
     */
    void showExceptionMessageBox(const QString& title, const QString& message);

#ifndef QT_NO_WIDGETS
private slots:
    /**
     * @brief 在主线程中显示异常消息框
     * @param title 标题
     * @param message 消息
     */
    void showExceptionMessageBoxInMainThread(const QString& title, const QString& message);
#endif
};

// 异常处理宏
#define HANDLE_EXCEPTION(code) ExceptionHandler::instance().handle([&]() { code; })
#define HANDLE_EXCEPTION_NO_MSG(code) ExceptionHandler::instance().handle([&]() { code; }, false)
#define HANDLE_EXCEPTION_WITH_RETURN(code, defaultValue) ExceptionHandler::instance().handleWithReturn([&]() { return code; }, defaultValue)
#define HANDLE_EXCEPTION_WITH_RETURN_NO_MSG(code, defaultValue) ExceptionHandler::instance().handleWithReturn([&]() { return code; }, defaultValue, false)

#endif // EXCEPTION_HANDLER_H 