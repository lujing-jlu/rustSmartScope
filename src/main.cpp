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
#include "camera_parameter_manager.h"
#include "qml_video_item.h"
#include "video_transform_manager.h"
#include <QQmlEngine>
#include "ai_detection_manager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("RustSmartScope");
    app.setApplicationVersion("0.1.0");

    // 初始化Rust核心（包含日志系统）
    int result = smartscope_init();
    if (result != SMARTSCOPE_ERROR_SUCCESS) {
        LOG_ERROR("Main", "Failed to initialize Rust core: ", smartscope_get_error_string(result));
        return -1;
    }

    // 加载根目录配置（仅使用 smartscope.toml；若不存在将使用默认配置）
    // 配置文件相对于项目根目录（可执行文件在 build/bin/ 下）
    const char* config_path = "smartscope.toml";
    result = smartscope_load_config(config_path);
    if (result != SMARTSCOPE_ERROR_SUCCESS) {
        LOG_ERROR("Main", "Failed to load config: ", smartscope_get_error_string(result));
    }

    // 启用配置文件热重载
    result = smartscope_enable_config_hot_reload(config_path);
    if (result != SMARTSCOPE_ERROR_SUCCESS) {
        LOG_WARN("Main", "Failed to enable config hot reload: ", smartscope_get_error_string(result));
    } else {
        LOG_INFO("Main", "配置文件热重载已启用，修改 smartscope.toml 将自动生效");
    }

    // 注册QML日志组件
    QmlLogger::registerQmlType();

    // 注册QmlVideoItem到QML
    qmlRegisterType<QmlVideoItem>("RustSmartScope.Video", 1, 0, "VideoDisplay");

    // 创建相机管理器
    CameraManager *cameraManager = new CameraManager(&app);

    // 创建相机参数管理器
    CameraParameterManager *cameraParameterManager = new CameraParameterManager(&app);

    // 创建视频变换管理器
    VideoTransformManager *videoTransformManager = new VideoTransformManager(&app);

    // 创建AI检测管理器并初始化
    AiDetectionManager *aiDetectionManager = new AiDetectionManager(&app);
    aiDetectionManager->initialize("models/yolov8m.rknn", 6);

    // 创建QML引擎
    QQmlApplicationEngine engine;

    // 设置窗口属性到QML上下文
    engine.rootContext()->setContextProperty("appVersion", "0.1.0");
    engine.rootContext()->setContextProperty("rustInitialized", smartscope_is_initialized());
    engine.rootContext()->setContextProperty("CameraManager", cameraManager);
    engine.rootContext()->setContextProperty("CameraParameterManager", cameraParameterManager);
    engine.rootContext()->setContextProperty("VideoTransformManager", videoTransformManager);
    engine.rootContext()->setContextProperty("AiDetectionManager", aiDetectionManager);

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

    // 连接相机输出到AI检测（显示用的pixmap即用于推理）
    QObject::connect(cameraManager, &CameraManager::leftPixmapUpdated,
                     aiDetectionManager, &AiDetectionManager::onLeftPixmap);
    QObject::connect(cameraManager, &CameraManager::singlePixmapUpdated,
                     aiDetectionManager, &AiDetectionManager::onSinglePixmap);

    // 运行应用程序
    int exitCode = app.exec();

    // 清理Rust资源
    smartscope_shutdown();

    return exitCode;
}
