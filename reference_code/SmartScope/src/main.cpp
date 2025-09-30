#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QStandardPaths>
#include <QSplashScreen>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QFontDatabase>
#include <QThread>
#include <QInputMethod>
#include <QStyleFactory>
#include "mainwindow.h"
#include "infrastructure/logging/logger.h"
#include "infrastructure/config/config_manager.h"
#include "inference/inference_service.hpp"
#include "inference/yolov8_service.hpp"

using namespace SmartScope::Infrastructure;

// 创建启动画面
QSplashScreen* createSplashScreen()
{
    // 获取屏幕信息，使用全屏尺寸
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int width = screenGeometry.width();  // 使用屏幕实际宽度
    int height = screenGeometry.height(); // 使用屏幕实际高度
    
    // 创建黑色背景图像，全屏显示
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::black);
    
    // 添加文字
    QPainter painter(&pixmap);
    painter.setPen(Qt::white);
    
    // 设置大号字体，根据屏幕尺寸调整字体大小
    int fontSize = qMin(width, height) / 10; // 根据屏幕尺寸动态计算字体大小
    QFont font("WenQuanYi Zen Hei", fontSize, QFont::Bold);
    painter.setFont(font);
    
    // 在中央绘制EDDYSUN文字
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "EDDYSUN");
    
    // 在底部绘制加载文字，字体大小也动态调整
    int smallFontSize = qMin(width, height) / 30;
    font.setPointSize(smallFontSize);
    painter.setFont(font);
    painter.drawText(QRect(0, height - height/8, width, height/10), Qt::AlignCenter, "正在启动，请稍候...");
    
    // 创建启动画面
    QSplashScreen* splash = new QSplashScreen(pixmap);
    splash->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    
    return splash;
}

// 初始化日志系统
bool initLogger()
{
    qDebug() << "初始化日志系统...";
    qDebug() << "应用程序路径:" << QCoreApplication::applicationDirPath();
    qDebug() << "当前工作目录:" << QDir::currentPath();
    
    // 使用当前工作目录作为日志目录
    QString logDir = QDir::currentPath() + "/logs";
    
    qDebug() << "使用日志目录:" << logDir;
    
    QDir dir(logDir);
    if (!dir.exists()) {
        qDebug() << "日志目录不存在，尝试创建...";
        if (!dir.mkpath(".")) {
            qWarning() << "无法创建日志目录:" << logDir << ", 错误:" << dir.exists();
            
            // 尝试使用临时目录
            logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/smartscope_logs";
            qDebug() << "尝试使用临时目录:" << logDir;
            
            dir = QDir(logDir);
            if (!dir.exists() && !dir.mkpath(".")) {
                qWarning() << "无法创建临时日志目录:" << logDir;
                return false;
            }
        }
    }
    
    // 设置日志文件路径
    QString logFilePath = logDir + "/app.log";
    qDebug() << "日志文件路径:" << logFilePath;
    
    // 测试文件可写性
    QFile testFile(logFilePath);
    if (testFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qDebug() << "日志文件可写";
        testFile.close();
    } else {
        qWarning() << "日志文件不可写:" << testFile.errorString();
        return false;
    }
    
    // 初始化日志系统，禁用文件输出，只使用控制台输出
    bool success = Logger::instance().init(logFilePath, LogLevel::INFO, true, false);
    if (success) {
        qDebug() << "日志系统初始化成功，仅使用控制台输出";
        LOG_INFO("应用程序启动成功");
        return true;
    } else {
        qWarning() << "日志系统初始化失败";
        return false;
    }
}

// 初始化配置管理器
bool initConfig()
{
    qDebug() << "初始化配置管理器...";
    
    try {
        // 初始化配置管理器
        bool success = ConfigManager::instance().init("");
        if (success) {
            qDebug() << "配置管理器初始化成功";
            
            // 设置一些基本配置
            CONFIG_SET_VALUE("app/version", QCoreApplication::applicationVersion());
            CONFIG_SET_VALUE("app/name", QCoreApplication::applicationName());
            CONFIG_SET_VALUE("ui/theme", "dark");
            CONFIG_SET_VALUE("ui/language", "zh_CN");
            
            // 尝试多个位置查找配置文件
            QStringList configPaths;
            configPaths << QDir::currentPath() + "/config.toml"
                       << QCoreApplication::applicationDirPath() + "/config.toml"
                       << QCoreApplication::applicationDirPath() + "/../config.toml";
            
            QString configPath;
            for (const QString& path : configPaths) {
                if (QFile::exists(path)) {
                    configPath = path;
                    qDebug() << "找到配置文件:" << configPath;
                    break;
                }
            }
            
            if (configPath.isEmpty()) {
                qWarning() << "配置文件不存在，尝试的路径:";
                for (const QString& path : configPaths) {
                    qWarning() << "  -" << path;
                }
                return true; // 即使没有配置文件，也返回成功
            }
            
            qDebug() << "加载配置文件:" << configPath;
            
            if (ConfigManager::instance().loadTomlConfig(configPath)) {
                qDebug() << "配置文件加载成功";
                
                // 解析相机配置
                QVariant leftCameraConfig = CONFIG_GET_VALUE("camera/left", QVariant());
                QVariant rightCameraConfig = CONFIG_GET_VALUE("camera/right", QVariant());
                QVariant swapLeftRightConfig = CONFIG_GET_VALUE("camera/swap_left_right", QVariant());
                QVariant leftUSBPositionConfig = CONFIG_GET_VALUE("camera/left/usb_position", QVariant());
                QVariant rightUSBPositionConfig = CONFIG_GET_VALUE("camera/right/usb_position", QVariant());
                
                if (leftCameraConfig.isValid() && leftCameraConfig.canConvert<QVariantMap>()) {
                    QVariantMap leftCameraMap = leftCameraConfig.toMap();
                    QString leftCameraName = leftCameraMap.value("name").toString();
                    QString leftCameraFriendlyName = leftCameraMap.value("friendly_name").toString();
                    qDebug() << "解析到的左相机名称:" << "(" << leftCameraName << ", " << leftCameraFriendlyName << ")";
                } else {
                    qWarning() << "左相机配置无效或格式错误";
                }
                
                if (rightCameraConfig.isValid() && rightCameraConfig.canConvert<QVariantMap>()) {
                    QVariantMap rightCameraMap = rightCameraConfig.toMap();
                    QString rightCameraName = rightCameraMap.value("name").toString();
                    QString rightCameraFriendlyName = rightCameraMap.value("friendly_name").toString();
                    qDebug() << "解析到的右相机名称:" << "(" << rightCameraName << ", " << rightCameraFriendlyName << ")";
            } else {
                    qWarning() << "右相机配置无效或格式错误";
            }
            
            LOG_INFO("配置管理器初始化成功");
            return true;
            } else {
                qWarning() << "配置文件加载失败:" << configPath;
                return false;
            }
        } else {
            qWarning() << "配置管理器初始化失败";
            return false;
        }
    } catch (const std::exception& e) {
        qWarning() << "配置管理器初始化异常:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "配置管理器初始化未知异常";
        return false;
    }
}

// 初始化YOLOv8服务
bool initYOLOv8Service()
{
    qDebug() << "初始化YOLOv8服务...";
    
    try {
        // 查找模型文件
        QString modelPath;
        QStringList possibleModelPaths;
        possibleModelPaths << QDir::currentPath() + "/models/yolov8m.rknn"
                          << QCoreApplication::applicationDirPath() + "/models/yolov8m.rknn"
                          << QCoreApplication::applicationDirPath() + "/../models/yolov8m.rknn";
        
        for (const QString& path : possibleModelPaths) {
            if (QFile::exists(path)) {
                modelPath = path;
                qDebug() << "找到YOLOv8模型文件:" << modelPath;
                break;
            }
        }
        
        if (modelPath.isEmpty()) {
            for (const QString& path : possibleModelPaths) {
                qWarning() << "YOLOv8模型文件不存在:" << path;
            }
            return false;
        }
        
        // 查找标签文件
        QString labelPath;
        QStringList possibleLabelPaths;
        possibleLabelPaths << QDir::currentPath() + "/models/coco_80_labels_list.txt"
                          << QCoreApplication::applicationDirPath() + "/models/coco_80_labels_list.txt"
                          << QCoreApplication::applicationDirPath() + "/../models/coco_80_labels_list.txt";
        
        for (const QString& path : possibleLabelPaths) {
            if (QFile::exists(path)) {
                labelPath = path;
                qDebug() << "找到YOLOv8标签文件:" << labelPath;
                break;
            }
        }
        
        if (labelPath.isEmpty()) {
            for (const QString& path : possibleLabelPaths) {
                qWarning() << "YOLOv8标签文件不存在:" << path;
            }
            return false;
        }
        
        // 初始化YOLOv8服务
        return SmartScope::YOLOv8Service::instance().initialize(modelPath, labelPath);
    } catch (const std::exception& e) {
        qWarning() << "初始化YOLOv8服务异常:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "初始化YOLOv8服务未知异常";
        return false;
    }
}

// 进行应用程序初始化
void initializeApp(QSplashScreen* splash)
{
    // 初始化日志系统
    if (!initLogger()) {
        qWarning() << "日志系统初始化失败";
    }
    
    // 初始化配置管理器
    if (!initConfig()) {
        qWarning() << "配置管理器初始化失败";
    }
    
    // 初始化YOLOv8服务
    if (!initYOLOv8Service()) {
        qWarning() << "YOLOv8服务初始化失败";
    }

    // 不再显示额外的启动提示
    QThread::msleep(100);
}

int main(int argc, char *argv[])
{
    // 注册自定义类型到Qt元对象系统
    qRegisterMetaType<SmartScope::InferenceResult>("InferenceResult");
    qRegisterMetaType<SmartScope::InferenceResult>("SmartScope::InferenceResult");
    qRegisterMetaType<SmartScope::YOLOv8Result>("YOLOv8Result");
    qRegisterMetaType<SmartScope::YOLOv8Result>("SmartScope::YOLOv8Result");
    
    // 设置虚拟键盘环境变量
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE", QByteArray("0"));
    
    // 设置应用程序属性
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    QApplication app(argc, argv);
    
    // 设置输入法策略
    app.setStyle(QStyleFactory::create("Fusion"));
    QGuiApplication::setOverrideCursor(Qt::ArrowCursor);
    // 重要修复：不要强制隐藏输入法！注释掉这行
    // QGuiApplication::inputMethod()->setVisible(false);

    // 设置全局输入法策略
    foreach (QScreen *screen, QGuiApplication::screens()) {
        screen->setOrientationUpdateMask(Qt::PrimaryOrientation);
    }
    
    // 设置应用程序信息
    app.setApplicationName("SmartScope");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SmartScope");
    app.setOrganizationDomain("smartscope.com");
    
    // 创建启动画面
    QSplashScreen* splash = createSplashScreen();
    splash->show();
    
    // 确保启动画面立即显示
    app.processEvents();
    
    // 在后台进行初始化
    initializeApp(splash);
    
    // 创建主窗口但不立即显示
    MainWindow* mainWindow = new MainWindow();

    // 使用定时器延迟显示主窗口，确保相机初始化有足够时间完成
    QTimer::singleShot(1000, [splash, mainWindow]() {
        // 关闭启动画面并显示主窗口
        splash->finish(mainWindow);
        mainWindow->show();
        mainWindow->raise();
        mainWindow->activateWindow();
        // 释放启动画面内存
        splash->deleteLater();
    });
    
    // 运行应用程序
    return app.exec();
} 