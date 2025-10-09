#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QTimer>

// 引入Rust FFI头文件
extern "C" {
    #include "smartscope.h"
}

// 引入统一日志系统
#include "logger.h"
#include "qml_logger.h"
#include "camera_manager.h"
#include "qml_video_item.h"
#include <QQmlEngine>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("RustSmartScope");
    app.setApplicationVersion("0.1.0");

    // 使用统一日志系统替代qDebug
    LOG_INFO("Main", "Starting RustSmartScope...");

    // 初始化Rust核心（包含日志系统）
    LOG_INFO("Main", "Initializing Rust core...");
    int result = smartscope_init();
    if (result != SMARTSCOPE_ERROR_SUCCESS) {
        LOG_ERROR("Main", "Failed to initialize Rust core: ", smartscope_get_error_string(result));
        return -1;
    }

    LOG_INFO("Main", "Rust core initialized successfully");
    LOG_INFO("Main", "Version: ", smartscope_get_version());
    LOG_INFO("Main", "Is initialized: ", smartscope_is_initialized() ? "true" : "false");

    // 测试配置文件操作
    LOG_INFO("Main", "Testing config operations...");
    const char* configPath = "test_config.toml";

    // 尝试保存配置
    result = smartscope_save_config(configPath);
    if (result == SMARTSCOPE_ERROR_SUCCESS) {
        LOG_INFO("Main", "Config saved successfully");

        // 尝试加载配置
        result = smartscope_load_config(configPath);
        if (result == SMARTSCOPE_ERROR_SUCCESS) {
            LOG_INFO("Main", "Config loaded successfully");
        } else {
            LOG_ERROR("Main", "Failed to load config: ", smartscope_get_error_string(result));
        }
    } else {
        LOG_ERROR("Main", "Failed to save config: ", smartscope_get_error_string(result));
    }

    // 注册QML日志组件
    QmlLogger::registerQmlType();

    // 注册QmlVideoItem到QML
    qmlRegisterType<QmlVideoItem>("RustSmartScope.Video", 1, 0, "VideoDisplay");

    // 创建相机管理器
    CameraManager *cameraManager = new CameraManager(&app);

    // 创建QML引擎
    QQmlApplicationEngine engine;

    // 设置窗口属性到QML上下文
    engine.rootContext()->setContextProperty("appVersion", "0.1.0");
    engine.rootContext()->setContextProperty("rustInitialized", smartscope_is_initialized());
    engine.rootContext()->setContextProperty("CameraManager", cameraManager);

    // 加载QML文件
    const QUrl qmlUrl(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [qmlUrl](QObject *obj, const QUrl &objUrl) {
        if (!obj && qmlUrl == objUrl) {
            LOG_ERROR("Main", "Failed to load QML file: ", qmlUrl.toString().toStdString());
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(qmlUrl);

    if (engine.rootObjects().isEmpty()) {
        LOG_ERROR("Main", "No QML root objects found");
        return -1;
    }

    LOG_INFO("Main", "Application started, showing window...");

    // 运行应用程序
    int exitCode = app.exec();

    // 清理Rust资源
    LOG_INFO("Main", "Shutting down Rust core...");
    smartscope_shutdown();

    LOG_INFO("Main", "Application exited with code: ", exitCode);
    return exitCode;
}