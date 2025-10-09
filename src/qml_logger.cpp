#include "qml_logger.h"
#include <QQmlEngine>

QmlLogger::QmlLogger(QObject *parent)
    : QObject(parent)
{
}

void QmlLogger::registerQmlType()
{
    qmlRegisterSingletonType<QmlLogger>("RustSmartScope.Logger", 1, 0, "Logger",
        [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject* {
            Q_UNUSED(engine)
            Q_UNUSED(scriptEngine)
            return new QmlLogger();
        });
}

void QmlLogger::trace(const QString& message)
{
    log(Logger::Trace, message);
}

void QmlLogger::debug(const QString& message)
{
    log(Logger::Debug, message);
}

void QmlLogger::info(const QString& message)
{
    log(Logger::Info, message);
}

void QmlLogger::warn(const QString& message)
{
    log(Logger::Warn, message);
}

void QmlLogger::error(const QString& message)
{
    log(Logger::Error, message);
}

void QmlLogger::setLevel(int level)
{
    if (level >= 0 && level <= 4) {
        Logger::setLevel(static_cast<Logger::Level>(level));
    }
}

void QmlLogger::log(Logger::Level level, const QString& message)
{
    Logger::qml(level, message.toStdString());
}