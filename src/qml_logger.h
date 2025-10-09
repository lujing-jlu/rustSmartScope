#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include "logger.h"

/// QML日志桥接类
class QmlLogger : public QObject
{
    Q_OBJECT

public:
    explicit QmlLogger(QObject *parent = nullptr);

    // 注册到QML系统
    static void registerQmlType();

public slots:
    /// QML调用的日志函数
    void trace(const QString& message);
    void debug(const QString& message);
    void info(const QString& message);
    void warn(const QString& message);
    void error(const QString& message);

    /// 设置日志级别
    void setLevel(int level);

private:
    void log(Logger::Level level, const QString& message);
};