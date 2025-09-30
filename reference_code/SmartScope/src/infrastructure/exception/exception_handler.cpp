#include "infrastructure/exception/exception_handler.h"
#include <QThread>
#include <QDebug>

#ifndef QT_NO_WIDGETS
#include <QMessageBox>
#include <QApplication>
#endif

ExceptionHandler::ExceptionHandler()
{
}

ExceptionHandler::~ExceptionHandler()
{
}

ExceptionHandler& ExceptionHandler::instance()
{
    static ExceptionHandler instance;
    return instance;
}

bool ExceptionHandler::handle(const std::function<void()>& func, bool showMessageBox)
{
    try {
        func();
        return true;
    } catch (const AppException& e) {
        handleAppException(e, showMessageBox);
    } catch (const std::exception& e) {
        handleStdException(e, showMessageBox);
    } catch (...) {
        handleUnknownException(showMessageBox);
    }
    return false;
}

void ExceptionHandler::handleAppException(const AppException& e, bool showMessageBox)
{
    QString message = e.getFormattedMessage();
    qCritical() << "应用异常:" << message;
    
    // 发送异常信号
    emit exceptionOccurred(e);
    
    // 显示消息框
    if (showMessageBox) {
        showExceptionMessageBox("应用异常", message);
    }
}

void ExceptionHandler::handleStdException(const std::exception& e, bool showMessageBox)
{
    QString message = QString("标准异常: %1").arg(e.what());
    qCritical() << message;
    
    // 显示消息框
    if (showMessageBox) {
        showExceptionMessageBox("标准异常", message);
    }
}

void ExceptionHandler::handleUnknownException(bool showMessageBox)
{
    QString message = "发生未知异常";
    qCritical() << message;
    
    // 显示消息框
    if (showMessageBox) {
        showExceptionMessageBox("未知异常", message);
    }
}

void ExceptionHandler::showExceptionMessageBox(const QString& title, const QString& message)
{
#ifndef QT_NO_WIDGETS
    // 确保在主线程中显示消息框
    if (QThread::currentThread() == QApplication::instance()->thread()) {
        QMessageBox::critical(nullptr, title, message);
    } else {
        // 在其他线程中，使用信号槽机制在主线程中显示消息框
        QMetaObject::invokeMethod(this, "showExceptionMessageBoxInMainThread",
                                 Qt::QueuedConnection,
                                 Q_ARG(QString, title),
                                 Q_ARG(QString, message));
    }
#else
    Q_UNUSED(title);
    Q_UNUSED(message);
    // 在非GUI环境下，只输出到控制台
    qCritical() << "异常:" << title << "-" << message;
#endif
}

#ifndef QT_NO_WIDGETS
void ExceptionHandler::showExceptionMessageBoxInMainThread(const QString& title, const QString& message)
{
    QMessageBox::critical(nullptr, title, message);
}
#endif 