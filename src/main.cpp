#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QTimer>

// 引入Rust FFI头文件
extern "C" {
    #include "smartscope.h"
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("RustSmartScope");
    app.setApplicationVersion("0.1.0");

    qDebug() << "Starting RustSmartScope...";

    // 测试Rust FFI连接
    qDebug() << "Initializing Rust core...";
    int result = smartscope_init();
    if (result != SMARTSCOPE_ERROR_SUCCESS) {
        qCritical() << "Failed to initialize Rust core:" << smartscope_get_error_string(result);
        return -1;
    }

    qDebug() << "Rust core initialized successfully";
    qDebug() << "Version:" << smartscope_get_version();
    qDebug() << "Is initialized:" << smartscope_is_initialized();

    // 测试配置文件操作
    qDebug() << "Testing config operations...";
    const char* configPath = "test_config.toml";

    // 尝试保存配置
    result = smartscope_save_config(configPath);
    if (result == SMARTSCOPE_ERROR_SUCCESS) {
        qDebug() << "Config saved successfully";

        // 尝试加载配置
        result = smartscope_load_config(configPath);
        if (result == SMARTSCOPE_ERROR_SUCCESS) {
            qDebug() << "Config loaded successfully";
        } else {
            qWarning() << "Failed to load config:" << smartscope_get_error_string(result);
        }
    } else {
        qWarning() << "Failed to save config:" << smartscope_get_error_string(result);
    }

    // 创建QML引擎显示空白页面
    QQmlApplicationEngine engine;

    // 设置窗口属性到QML上下文
    engine.rootContext()->setContextProperty("appVersion", "0.1.0");
    engine.rootContext()->setContextProperty("rustInitialized", smartscope_is_initialized());

    // 加载QML文件
    const QUrl qmlUrl(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [qmlUrl](QObject *obj, const QUrl &objUrl) {
        if (!obj && qmlUrl == objUrl) {
            qCritical() << "Failed to load QML file:" << qmlUrl;
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(qmlUrl);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No QML root objects found";
        return -1;
    }

    qDebug() << "Application started, showing window...";

    // 运行应用程序
    int exitCode = app.exec();

    // 清理Rust资源
    qDebug() << "Shutting down Rust core...";
    smartscope_shutdown();

    qDebug() << "Application exited with code:" << exitCode;
    return exitCode;
}