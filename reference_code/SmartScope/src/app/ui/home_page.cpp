#include "app/ui/home_page.h"
#include "core/camera/camera_correction_factory.h"
#include <QLabel>
#include <QToolButton>
#include <QStyle>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QResizeEvent>
#include <QApplication>
#include <QScreen>
#include <QMessageBox>
#include <QProcess>
#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QGroupBox>
#include <QCheckBox>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QTime>
#include <QRandomGenerator>
#include <QDebug>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QThread>
#include <QFileDialog>
#include <QPropertyAnimation>
#include <iostream>
#include <chrono>
// 添加配置管理器头文件
#include "infrastructure/config/config_manager.h"
#include "app/ui/toast_notification.h"
// 添加日志记录器头文件
#include "infrastructure/logging/logger.h"
// 添加状态栏头文件
#include "statusbar.h"
#include "mainwindow.h"
#include "app/ui/page_manager.h"
#include "app/utils/screenshot_manager.h"
// 添加LED控制器头文件
#include "app/utils/led_controller.h"
// 添加键盘监听器头文件
#include "app/utils/keyboard_listener.h"
// 添加YOLOv8服务头文件
#include "inference/yolov8_service.hpp"

// 更新命名空间引用
using namespace SmartScope::Core::CameraUtils;
using namespace SmartScope::Infrastructure;
using namespace SmartScope;  // 添加这一行，以便使用 StatusBar 和 PathSelector

// 定义客户端ID
const std::string HomePage::CLIENT_ID = "HomePage";

// 定义检测间隔常量
const int DETECTION_INTERVAL_MS = 200;

bool HomePage::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        QGestureEvent* ge = static_cast<QGestureEvent*>(event);
        if (QGesture* g = ge->gesture(Qt::PinchGesture)) {
            QPinchGesture* pg = static_cast<QPinchGesture*>(g);
            if (pg->state() == Qt::GestureStarted) {
                m_zoomScaleInitial = m_zoomScale;
            }
            const QPinchGesture::ChangeFlags cf = pg->changeFlags();
            if (cf & QPinchGesture::ScaleFactorChanged) {
                double factor = pg->scaleFactor();
                m_zoomScale = std::clamp(m_zoomScaleInitial * factor, 0.1, 5.0);
            }
            ge->accept(pg);
            return true;
        }
    }
    return BasePage::event(event);
}

HomePage::HomePage(QWidget *parent)
    : BasePage("智能双目测量系统", parent),
      m_leftCameraView(nullptr),
      m_rightCameraView(nullptr),
      m_leftFrame(),
      m_rightFrame(),
      m_leftFrameTimestamp(0),
      m_rightFrameTimestamp(0),
      m_dragStartPosition(),
      m_camerasInitialized(false),
      m_updateTimer(nullptr),
      m_pathSelector(nullptr),
      m_currentWorkPath(""),
      m_screenshotManager(nullptr),
      m_captureDebounceTimer(nullptr),
      m_isCapturing(false),
      m_adjustmentPanel(nullptr),
      m_adjustmentPanelVisible(false),
      m_rgaPanel(nullptr),
      m_rgaPanelVisible(false),
      m_cameraMode(CameraMode::NO_CAMERA),
      m_detectionEnabled(false),
      m_detectionInProgress(false),
      m_lastDetectionFrame(),
      m_lastDetectionCameraId(""),
      m_processingDetection(false),
      m_lastDetectionTime(std::chrono::steady_clock::now()),
      m_lastDetectionSessionId(0),
      m_detectionConfidenceThreshold(0.1f),  // 降低置信度阈值到10%
      m_correctionManager(nullptr),
      m_distortionCorrectionEnabled(false)
{
    LOG_INFO("创建主页实例");
    
    // 安装事件过滤器
    installEventFilter(this);
    
    // 创建截图管理器
    m_screenshotManager = new SmartScope::App::Utils::ScreenshotManager(this);
    
    // 初始化相机校正管理器
    m_correctionManager = SmartScope::Core::CameraCorrectionFactory::createStandardCorrectionManager(this);

    // 创建拍照防抖定时器
    m_captureDebounceTimer = new QTimer(this);
    m_captureDebounceTimer->setSingleShot(true);
    m_captureDebounceTimer->setInterval(1000); // 1秒防抖间隔
    connect(m_captureDebounceTimer, &QTimer::timeout, this, [this]() {
        m_isCapturing = false;
        LOG_DEBUG("拍照防抖定时器超时，重置拍照状态");
    });

    // 创建更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(33); // 约30FPS
    connect(m_updateTimer, &QTimer::timeout, this, &HomePage::updateCameraViews);
    
    // 初始化内容
    initContent();
    
    // 初始化相机
    QTimer::singleShot(500, this, &HomePage::initCameras);
    
    // 创建调节面板
    createAdjustmentPanel();
    
    // 初始化工具栏按钮
    initToolBarButtons();

    // 启用捏合手势
    grabGesture(Qt::PinchGesture);
    
    // 注册F9键为拍照快捷键
    KeyboardListener::instance().registerKeyHandler(Qt::Key_F9, [this]() {
        LOG_INFO("F9键被按下，触发拍照功能");
        captureAndSaveImages();
        return true;  // 表示事件已处理
    }, this);
    
    // 注册F12键为LED亮度控制快捷键
    KeyboardListener::instance().registerKeyHandler(Qt::Key_F12, [this]() {
        LOG_INFO("F12键被按下，触发LED亮度控制");
        
        if (LedController::instance().isConnected()) {
            if (LedController::instance().toggleBrightness()) {
                // 获取当前亮度百分比
                int percent = LedController::instance().getCurrentBrightnessPercentage();
                int level = LedController::instance().getCurrentLevelIndex();
                
                // 显示提示信息
                if (percent > 0) {
                    showToast(this, QString("灯光亮度：%1%").arg(percent), 1500);
                } else {
                    showToast(this, "灯光已关闭", 1500);
                }
                LOG_INFO(QString("灯光亮度已切换到级别 %1 (%2%)").arg(level).arg(percent));
            } else {
                showToast(this, "灯光控制失败", 2000);
                LOG_WARNING("灯光亮度切换失败");
            }
        } else {
            showToast(this, "未连接到LED控制设备", 2000);
            LOG_WARNING("LED控制器未连接到设备");
        }
        
        return true;  // 表示事件已处理
    }, this);
    
    // 页面加载后适当延迟显示LED亮度状态
    QTimer::singleShot(2000, this, [this]() {
        if (LedController::instance().isConnected()) {
            int percent = LedController::instance().getCurrentBrightnessPercentage();
            int level = LedController::instance().getCurrentLevelIndex();
            if (percent > 0) {
                showToast(this, QString("当前灯光亮度：%1%").arg(percent), 2000);
                LOG_INFO(QString("显示初始灯光亮度: %1%").arg(percent));
            }
        }
    });
    
    // 连接YOLOv8检测完成信号
    connect(&YOLOv8Service::instance(), &YOLOv8Service::detectionCompleted, 
            this, &HomePage::onDetectionCompleted, Qt::QueuedConnection);
    
    LOG_INFO("已连接YOLOv8检测完成信号");
}

HomePage::~HomePage()
{
    // 停止更新定时器
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    
    // 禁用相机
    disableCameras();
    
    // 取消注册F9键处理函数
    KeyboardListener::instance().unregisterKeyHandler(Qt::Key_F9, this);
    
    // 取消注册F12键处理函数
    KeyboardListener::instance().unregisterKeyHandler(Qt::Key_F12, this);
}

QString HomePage::getCurrentWorkPath() const
{
    return m_currentWorkPath;
}

void HomePage::setCurrentWorkPath(const QString &path)
{
    if (m_currentWorkPath != path) {
        m_currentWorkPath = path;
        LOG_INFO(QString("当前工作路径已更改为: %1").arg(path));
        
        // 发射信号
        emit currentWorkPathChanged(path);
        
        // 这里可以添加其他处理逻辑，例如保存到配置文件等
    }
}

void HomePage::onWorkPathChanged(const QString &path)
{
    setCurrentWorkPath(path);
    
    // 显示提示
    showToast(this, QString("当前工作路径: %1").arg(path), 2000);
}

// 添加初始化工具栏按钮的方法
void HomePage::initToolBarButtons()
{
    // 获取主窗口
    MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
    if (!mainWindow) {
        LOG_WARNING("无法获取主窗口，无法初始化工具栏按钮");
        return;
    }
    
    // 获取工具栏
    ToolBar* toolBar = mainWindow->getToolBar();
    if (!toolBar) {
        LOG_WARNING("无法获取工具栏，无法初始化工具栏按钮");
        return;
    }
    
    // 先检查按钮是否已存在，如果存在则先移除
    if (toolBar->getButton("adjustButton")) {
        LOG_INFO("相机调节按钮已存在，先移除");
        toolBar->removeButton("adjustButton");
    }
    
    if (toolBar->getButton("captureButton")) {
        LOG_INFO("截图按钮已存在，先移除");
        toolBar->removeButton("captureButton");
    }
    
    if (toolBar->getButton("screenshotButton")) {
        LOG_INFO("屏幕截图按钮已存在，先移除");
        toolBar->removeButton("screenshotButton");
    }
    
    if (toolBar->getButton("ledControlButton")) {
        LOG_INFO("LED控制按钮已存在，先移除");
        toolBar->removeButton("ledControlButton");
    }
    
    if (toolBar->getButton("detectionButton")) {
        LOG_INFO("目标检测按钮已存在，先移除");
        toolBar->removeButton("detectionButton");
    }
    
    // 添加相机调节按钮
    QPushButton* adjustButton = toolBar->addButton("adjustButton", ":/icons/config.svg", "画面调整", 0);
    if (adjustButton) {
        // 断开所有之前的连接
        disconnect(adjustButton, &QPushButton::clicked, nullptr, nullptr);
        
        // 重新连接信号
        connect(adjustButton, &QPushButton::clicked, this, [this]() {
            LOG_INFO("画面调整按钮被点击");
            toggleRgaPanel();
        });
        
        LOG_INFO("画面调整按钮已添加到工具栏");
    }
    
    // 添加截图按钮
    QPushButton* captureButton = toolBar->addButton("captureButton", ":/icons/camera.svg", "截图 (F9)", 1);
    if (captureButton) {
        // 断开所有之前的连接
        disconnect(captureButton, &QPushButton::clicked, nullptr, nullptr);
        
        // 重新连接信号
        connect(captureButton, &QPushButton::clicked, this, [this]() {
            LOG_INFO("截图按钮被点击");
            captureAndSaveImages();
        });
        
        LOG_INFO("截图按钮已添加到工具栏");
    }
    
    // 添加屏幕截图按钮
    QPushButton* screenshotButton = toolBar->addButton("screenshotButton", ":/icons/screenshot.svg", "屏幕截图", 2);
    if (screenshotButton) {
        // 断开所有之前的连接
        disconnect(screenshotButton, &QPushButton::clicked, nullptr, nullptr);
        
        // 重新连接信号
        connect(screenshotButton, &QPushButton::clicked, this, [this]() {
            LOG_INFO("屏幕截图按钮被点击");
            if (m_screenshotManager) {
                if (m_screenshotManager->captureFullScreen()) {
                    QString path = m_screenshotManager->getLastScreenshotPath();
                    showToast(this, QString("屏幕截图已保存至: %1").arg(path), 2000);
                    LOG_INFO(QString("屏幕截图已保存至: %1").arg(path));
                } else {
                    showToast(this, "屏幕截图保存失败", 2000);
                    LOG_WARNING("屏幕截图保存失败");
                }
            }
        });
        
        LOG_INFO("屏幕截图按钮已添加到工具栏");
    }
    
    // 添加LED控制按钮
    QPushButton* ledControlButton = toolBar->addButton("ledControlButton", ":/icons/brightness.svg", "LED控制 (F12)", 3);
    if (ledControlButton) {
        // 断开所有之前的连接
        disconnect(ledControlButton, &QPushButton::clicked, nullptr, nullptr);
        
        // 重新连接信号
        connect(ledControlButton, &QPushButton::clicked, this, [this]() {
            LOG_INFO("LED控制按钮被点击");
            
            if (LedController::instance().isConnected()) {
                if (LedController::instance().toggleBrightness()) {
                    // 获取当前亮度百分比
                    int percent = LedController::instance().getCurrentBrightnessPercentage();
                    
                    // 显示提示信息
                    if (percent > 0) {
                        showToast(this, QString("灯光亮度：%1%").arg(percent), 1500);
                    } else {
                        showToast(this, "灯光已关闭", 1500);
                    }
                } else {
                    showToast(this, "灯光控制失败", 2000);
                }
            } else {
                showToast(this, "未连接到LED控制设备", 2000);
            }
        });
        
        LOG_INFO("LED控制按钮已添加到工具栏");
    }
    
    // 添加目标检测按钮
    QPushButton* detectionButton = toolBar->addButton("detectionButton", ":/icons/AI.svg", "目标检测", 4);
    if (detectionButton) {
        // 设置为可选中按钮
        detectionButton->setCheckable(true);
        detectionButton->setChecked(m_detectionEnabled);
        
        // 断开所有之前的连接
        disconnect(detectionButton, &QPushButton::clicked, nullptr, nullptr);
        
        // 重新连接信号
        connect(detectionButton, &QPushButton::toggled, this, [this](bool checked) {
            LOG_INFO(QString("目标检测按钮状态: %1").arg(checked ? "启用" : "禁用"));
            toggleObjectDetection(checked);
        });
        
        LOG_INFO("目标检测按钮已添加到工具栏");
    }
    
    // 连接页面变化信号，控制工具栏按钮的可见性
    PageManager* pageManager = mainWindow->findChild<PageManager*>();
    if (pageManager) {
        // 先断开可能的旧连接，避免重复
        disconnect(pageManager, &PageManager::pageChanged, this, nullptr);
        
        // 连接页面变化信号
        connect(pageManager, &PageManager::pageChanged, this, [this, toolBar](PageType pageType) {
            if (!toolBar) return; // 防御性检查
            
            if (pageType == PageType::Home) {
                // 在主页显示主页相关按钮
                LOG_INFO("切换到主页，显示主页工具栏按钮");
                toolBar->showButton("adjustButton");
                toolBar->showButton("captureButton");
                toolBar->showButton("screenshotButton");
                toolBar->showButton("ledControlButton");
                toolBar->showButton("detectionButton");
            } else {
                // 在其他页面隐藏主页专用按钮，只保留屏幕截图
                LOG_INFO("切换到其他页面，隐藏主页专用按钮");
                toolBar->hideButton("adjustButton");
                toolBar->hideButton("captureButton");  // 隐藏拍照按钮，避免在无相机页面误触发
                toolBar->showButton("screenshotButton");  // 保留屏幕截图功能
                toolBar->hideButton("ledControlButton");
                toolBar->hideButton("detectionButton");
            }
        });
        
        LOG_INFO("已连接页面变化信号以控制工具栏按钮可见性");
    } else {
        LOG_WARNING("无法获取页面管理器，无法连接页面变化信号");
    }
}

void HomePage::initContent()
{
    // 创建网格布局
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);
    
    // 创建相机标签
    m_leftCameraView = new QLabel(this);
    m_leftCameraView->setObjectName("leftCameraView");
    m_leftCameraView->setAlignment(Qt::AlignCenter);
    m_leftCameraView->setMinimumSize(640, 480);
    m_leftCameraView->setStyleSheet("background-color: #1E1E1E; border: none;");
    
    // 修改：右相机视图标签现在用于显示左相机的画中画
    m_rightCameraView = new QLabel(this);
    m_rightCameraView->setObjectName("pipView"); // 更改对象名以反映其用途
    m_rightCameraView->setAlignment(Qt::AlignCenter);
    m_rightCameraView->setMinimumSize(320, 180); // 画中画尺寸
    m_rightCameraView->setStyleSheet("background-color: #1E1E1E; border: 2px solid white;"); // 添加边框以区分
    
    // 添加左相机标签到布局（全屏显示）
    gridLayout->addWidget(m_leftCameraView, 0, 0);
    
    // 设置布局
    m_contentLayout->addLayout(gridLayout);
    
    // 将画中画标签设置为浮动模式
    m_rightCameraView->setParent(this);
    m_rightCameraView->setFixedSize(320, 180);
    m_rightCameraView->move(20, 90); // 左上角位置
    m_rightCameraView->raise();
    m_rightCameraView->show();
    
    // 为相机标签安装事件过滤器，以便处理点击事件
    m_leftCameraView->installEventFilter(this);
    m_rightCameraView->installEventFilter(this);
    
    // 从配置文件获取根目录（默认 ~/data）
    QString rootDirectory = ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
    
    // 确保根目录存在
    QDir rootDir(rootDirectory);
    if (!rootDir.exists()) {
        rootDir.mkpath(".");
    }
    
    // 构建默认路径：根目录/Pictures
    m_currentWorkPath = rootDirectory + "/Pictures";
    {
        QDir picturesDir(m_currentWorkPath);
        if (!picturesDir.exists()) {
            picturesDir.mkpath(".");
            LOG_INFO(QString("创建图片目录: %1").arg(m_currentWorkPath));
        }
    }
    
    // 获取状态栏中的路径选择器
    SmartScope::PathSelector* pathSelector = nullptr;
    QWidget* mainWindow = this->window();
    if (mainWindow) {
        // 查找主窗口中的状态栏
        QList<SmartScope::StatusBar*> statusBars = mainWindow->findChildren<SmartScope::StatusBar*>();
        if (!statusBars.isEmpty()) {
            // 获取第一个状态栏中的路径选择器
            SmartScope::StatusBar* statusBar = statusBars.first();
            pathSelector = statusBar->getPathSelector();
        }
    }
    
    // 如果找到了路径选择器，设置当前路径并连接信号
    if (pathSelector) {
        // 设置路径选择器的当前路径
        pathSelector->setCurrentPath(m_currentWorkPath);
        
        // 连接路径选择器的信号
        connect(pathSelector, &SmartScope::PathSelector::pathChanged, this, &HomePage::onWorkPathChanged);
        
        LOG_INFO("已连接状态栏中的路径选择器");
        m_pathSelector = pathSelector;
    } else {
        LOG_WARNING("未找到状态栏中的路径选择器");
    }
    
    LOG_INFO("主页内容初始化完成");
    
    // 初始化调节面板
    createAdjustmentPanel();
    
    // 初始化工具栏按钮
    initToolBarButtons();
}

// 查找相机设备路径
QString HomePage::findCameraDevice(const QStringList& cameraNames)
{
    QProcess process;
    process.start("v4l2-ctl", QStringList() << "--list-devices");
    
    // 添加超时处理
    if (!process.waitForFinished(3000)) {
        LOG_ERROR("v4l2-ctl命令执行超时或失败");
        process.kill();
        return QString();
    }
    
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString error = QString::fromLocal8Bit(process.readAllStandardError());
    
    if (!error.isEmpty()) {
        LOG_WARNING("v4l2-ctl命令错误输出: " + error);
    }
    
    if (output.isEmpty()) {
        LOG_ERROR("v4l2-ctl命令未返回任何输出");
        return QString();
    }
    
    LOG_DEBUG("v4l2-ctl输出: " + output);
    
    // 解析v4l2-ctl输出
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    QMap<QString, QString> deviceMap; // 设备名称到第一个节点路径的映射
    QString currentDevice;
    
    // 首先构建设备名称到路径的映射
    for (int i = 0; i < lines.size(); i++) {
        QString line = lines[i].trimmed();
        
        // 如果不是设备路径行，则认为是设备名称行
        if (!line.startsWith("/dev/")) {
            currentDevice = line;
            LOG_DEBUG(QString("发现设备: %1").arg(currentDevice));
            continue;
        }
        
        // 如果是设备路径行且有当前设备名称，且该设备还没有映射过（只取第一个节点）
        if (line.startsWith("/dev/video") && !currentDevice.isEmpty() && !deviceMap.contains(currentDevice)) {
            deviceMap[currentDevice] = line;
            LOG_DEBUG(QString("映射设备: %1 -> %2").arg(currentDevice).arg(line));
        }
    }
    
    // 遍历所有提供的相机名称，查找匹配项
    for (const QString& cameraName : cameraNames) {
        LOG_DEBUG(QString("尝试匹配相机名称: %1").arg(cameraName));
        
        // 遍历所有找到的设备
        for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
            // 检查设备名称是否包含我们要找的相机名称（不区分大小写）
            if (it.key().contains(cameraName, Qt::CaseInsensitive)) {
                LOG_INFO(QString("找到匹配的相机: %1 -> %2").arg(cameraName).arg(it.value()));
                return it.value();
                }
        }
    }
    
    // 如果没有找到精确匹配，尝试模糊匹配
    for (const QString& cameraName : cameraNames) {
        // 检查是否是左相机或右相机的关键词
        bool isLeftCamera = cameraName.contains("cameraL", Qt::CaseInsensitive) || 
                            cameraName.contains("left", Qt::CaseInsensitive) || 
                            cameraName.contains("左", Qt::CaseInsensitive);
                            
        bool isRightCamera = cameraName.contains("cameraR", Qt::CaseInsensitive) || 
                             cameraName.contains("right", Qt::CaseInsensitive) || 
                             cameraName.contains("右", Qt::CaseInsensitive);
        
        // 如果是左相机或右相机，尝试查找相应的设备
        for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
            if ((isLeftCamera && (it.key().contains("left", Qt::CaseInsensitive) || 
                                  it.key().contains("左", Qt::CaseInsensitive) ||
                                  it.key().contains("cameraL", Qt::CaseInsensitive))) ||
                (isRightCamera && (it.key().contains("right", Qt::CaseInsensitive) || 
                                   it.key().contains("右", Qt::CaseInsensitive) ||
                                   it.key().contains("cameraR", Qt::CaseInsensitive)))) {
                LOG_INFO(QString("找到模糊匹配的相机: %1 -> %2").arg(cameraName).arg(it.value()));
                return it.value();
                }
        }
    }
    
    // 如果仍然没有找到，尝试使用任何可用的视频设备
    if (!deviceMap.isEmpty()) {
        QString firstDevice = deviceMap.values().first();
        LOG_WARNING(QString("未找到匹配的相机设备，使用第一个可用设备: %1").arg(firstDevice));
        return firstDevice;
    }
    
    LOG_WARNING("未找到任何相机设备");
    return QString();
}

// 获取所有可用的相机设备
QStringList HomePage::getAllAvailableCameras()
{
    QStringList availableCameras;

    QProcess process;
    process.start("v4l2-ctl", QStringList() << "--list-devices");

    // 添加超时处理
    if (!process.waitForFinished(3000)) {
        LOG_ERROR("v4l2-ctl命令执行超时或失败");
        process.kill();
        return availableCameras;
    }

    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString error = QString::fromLocal8Bit(process.readAllStandardError());

    if (!error.isEmpty()) {
        LOG_WARNING("v4l2-ctl命令错误输出: " + error);
    }

    if (output.isEmpty()) {
        LOG_ERROR("v4l2-ctl命令未返回任何输出");
        return availableCameras;
    }

    LOG_DEBUG("v4l2-ctl输出: " + output);

    // 解析v4l2-ctl输出，按设备分组
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    // 需要过滤的非相机设备关键词
    QStringList excludeKeywords = {
        "rk_hdmirx",        // HDMI接收器
        "hdmirx",           // HDMI相关
        "hdmi",             // HDMI设备
        "capture",          // 一些系统捕获设备
        "loopback"          // 虚拟设备
    };

    QString currentDeviceName;
    QStringList currentDeviceVideos;

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();

        // 检查是否是设备名称行（不以/dev/开头且包含设备描述）
        if (!trimmedLine.startsWith("/dev/") && trimmedLine.contains("(") && trimmedLine.contains(")")) {
            // 处理上一个设备的video节点
            if (!currentDeviceName.isEmpty() && !currentDeviceVideos.isEmpty()) {
                // 检查设备名称是否需要过滤
                bool shouldExclude = false;
                for (const QString& keyword : excludeKeywords) {
                    if (currentDeviceName.toLower().contains(keyword.toLower())) {
                        shouldExclude = true;
                        LOG_DEBUG(QString("过滤非相机设备: %1 (包含关键词: %2)").arg(currentDeviceName).arg(keyword));
                        break;
                    }
                }

                if (!shouldExclude) {
                    // 对于真正的相机设备，只使用第一个video节点
                    QString firstVideo = currentDeviceVideos.first();
                    availableCameras.append(firstVideo);
                    LOG_INFO(QString("发现相机设备: %1 -> %2 (共%3个节点，使用第一个)")
                            .arg(currentDeviceName).arg(firstVideo).arg(currentDeviceVideos.size()));
                }
            }

            // 开始新设备
            currentDeviceName = trimmedLine;
            currentDeviceVideos.clear();
        }
        // 检查是否是video设备路径
        else if (trimmedLine.startsWith("/dev/video")) {
            currentDeviceVideos.append(trimmedLine);
        }
    }

    // 处理最后一个设备
    if (!currentDeviceName.isEmpty() && !currentDeviceVideos.isEmpty()) {
        bool shouldExclude = false;
        for (const QString& keyword : excludeKeywords) {
            if (currentDeviceName.toLower().contains(keyword.toLower())) {
                shouldExclude = true;
                LOG_DEBUG(QString("过滤非相机设备: %1 (包含关键词: %2)").arg(currentDeviceName).arg(keyword));
                break;
            }
        }

        if (!shouldExclude) {
            QString firstVideo = currentDeviceVideos.first();
            availableCameras.append(firstVideo);
            LOG_INFO(QString("发现相机设备: %1 -> %2 (共%3个节点，使用第一个)")
                    .arg(currentDeviceName).arg(firstVideo).arg(currentDeviceVideos.size()));
        }
    }

    LOG_INFO(QString("总共发现 %1 个有效相机设备").arg(availableCameras.size()));
    return availableCameras;
}

// 智能检测相机模式并初始化
void HomePage::smartCameraDetection()
{
    LOG_INFO("开始智能相机检测...");

    try {
        // 获取所有可用的相机设备
        QStringList availableCameras = getAllAvailableCameras();

        if (availableCameras.isEmpty()) {
            LOG_INFO("未检测到任何相机设备，进入无相机模式");
            m_cameraMode = CameraMode::NO_CAMERA;
            return;
        }

        LOG_INFO(QString("检测到 %1 个相机设备: %2").arg(availableCameras.size()).arg(availableCameras.join(", ")));

        // 尝试检测双相机模式
        // 从配置文件获取相机名称列表
        QVariant leftCameraNames = ConfigManager::instance().getValue("camera/left/name");
        QVariant rightCameraNames = ConfigManager::instance().getValue("camera/right/name");

        QStringList leftNames, rightNames;

        // 处理左相机名称列表
        if (leftCameraNames.type() == QVariant::List) {
            QVariantList list = leftCameraNames.toList();
            for (const QVariant& item : list) {
                leftNames.append(item.toString());
            }
        } else if (leftCameraNames.type() == QVariant::String) {
            leftNames.append(leftCameraNames.toString());
        }

        // 处理右相机名称列表
        if (rightCameraNames.type() == QVariant::List) {
            QVariantList list = rightCameraNames.toList();
            for (const QVariant& item : list) {
                rightNames.append(item.toString());
            }
        } else if (rightCameraNames.type() == QVariant::String) {
            rightNames.append(rightCameraNames.toString());
        }

        // 如果配置中没有相机名称，使用硬编码的默认值进行测试
        if (leftNames.isEmpty()) {
            leftNames << "YTXB: YTXB (usb-fc800000.usb-1.3)" << "cameraL" << "Web Camera 2Ks";
        }

        if (rightNames.isEmpty()) {
            rightNames << "YTXB: YTXB (usb-fc880000.usb-1.4.3)" << "cameraR" << "USB Camera";
        }

        // 尝试查找左右相机
        QString leftCameraId = findCameraDevice(leftNames);
        QString rightCameraId = findCameraDevice(rightNames);

        // 检查是否能进入双相机模式
        if (!leftCameraId.isEmpty() && !rightCameraId.isEmpty() && leftCameraId != rightCameraId) {
            LOG_INFO(QString("检测到双相机模式: 左相机=%1, 右相机=%2").arg(leftCameraId).arg(rightCameraId));
            m_cameraMode = CameraMode::DUAL_CAMERA;
            m_leftCameraId = leftCameraId;
            m_rightCameraId = rightCameraId;

            // 发射相机模式变更信号（双相机模式）
            emit cameraModeChanged(false);

            initDualCameraMode();
            return;
        }

        // 如果无法进入双相机模式，检查是否有任何可用的相机
        if (availableCameras.size() >= 1) {
            LOG_INFO(QString("进入单相机模式，使用设备: %1").arg(availableCameras.first()));
            m_cameraMode = CameraMode::SINGLE_CAMERA;
            m_leftCameraId = availableCameras.first();  // 使用第一个可用的相机作为主相机
            m_rightCameraId = "";  // 清空右相机ID

            // 发射相机模式变更信号（单相机模式）
            emit cameraModeChanged(true);

            initSingleCameraMode();
            return;
        }

        // 如果没有任何可用相机
        LOG_INFO("未检测到可用的相机设备，进入无相机模式");
        m_cameraMode = CameraMode::NO_CAMERA;

        // 发射相机模式变更信号（无相机模式，视为单相机模式处理）
        emit cameraModeChanged(true);

    } catch (const std::exception& e) {
        LOG_ERROR(QString("智能相机检测时发生异常: %1").arg(e.what()));
        m_cameraMode = CameraMode::NO_CAMERA;
    } catch (...) {
        LOG_ERROR("智能相机检测时发生未知异常");
        m_cameraMode = CameraMode::NO_CAMERA;
    }
}

// 初始化双相机模式
void HomePage::initDualCameraMode()
{
    LOG_INFO("初始化双相机模式...");

    try {
        // 获取相机支持的分辨率
        std::vector<cv::Size> leftResolutions = getSupportedResolutions(m_leftCameraId.toStdString());
        std::vector<cv::Size> rightResolutions = getSupportedResolutions(m_rightCameraId.toStdString());

        // 如果无法获取分辨率信息，使用默认值
        if (leftResolutions.empty() || rightResolutions.empty()) {
            LOG_WARNING("无法获取相机分辨率信息，使用默认值: 1280x720");
            leftResolutions.push_back(cv::Size(1280, 720));
            rightResolutions.push_back(cv::Size(1280, 720));
        }

        // 选择最佳分辨率 (优先选择1280x720)
        cv::Size bestResolution = {1280, 720}; // 默认值
        bool found = false;

        // 寻找两个相机都支持的1280x720分辨率
        for (const auto& leftRes : leftResolutions) {
            if (leftRes.width == 1280 && leftRes.height == 720) {
                for (const auto& rightRes : rightResolutions) {
                    if (rightRes.width == 1280 && rightRes.height == 720) {
                        bestResolution = leftRes;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // 如果没有找到1280x720，选择两个相机共同支持的最高分辨率
        if (!found) {
            for (const auto& leftRes : leftResolutions) {
                for (const auto& rightRes : rightResolutions) {
                    if (leftRes.width == rightRes.width && leftRes.height == rightRes.height) {
                        // 选择面积更大的分辨率
                        if (leftRes.width * leftRes.height > bestResolution.width * bestResolution.height) {
                            bestResolution = leftRes;
                            found = true;
                        }
                    }
                }
            }
        }

        LOG_INFO(QString("双相机模式选择的分辨率: %1x%2").arg(bestResolution.width).arg(bestResolution.height));

        // 获取支持的帧率
        std::vector<double> leftFps = getSupportedFrameRates(m_leftCameraId.toStdString(), bestResolution);
        std::vector<double> rightFps = getSupportedFrameRates(m_rightCameraId.toStdString(), bestResolution);

        // 如果无法获取帧率信息，使用默认值
        if (leftFps.empty() || rightFps.empty()) {
            LOG_WARNING("无法获取相机帧率信息，使用默认值: 30fps");
            leftFps.push_back(30.0);
            rightFps.push_back(30.0);
        }

        // 选择两个相机都支持的最佳帧率 (优先选择30fps)
        double bestFps = 30.0; // 默认值
        found = false;

        // 寻找两个相机都支持的30fps
        for (double leftRate : leftFps) {
            if (std::abs(leftRate - 30.0) < 1.0) {
                for (double rightRate : rightFps) {
                    if (std::abs(rightRate - 30.0) < 1.0) {
                        bestFps = 30.0;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // 如果没有找到30fps，选择两个相机共同支持的最高帧率
        if (!found) {
            for (double leftRate : leftFps) {
                for (double rightRate : rightFps) {
                    if (std::abs(leftRate - rightRate) < 1.0) {
                        if (leftRate > bestFps) {
                            bestFps = leftRate;
                            found = true;
                        }
                    }
                }
            }
        }

        LOG_INFO(QString("双相机模式选择的帧率: %1").arg(bestFps));

        // 设置相机配置
        int mjpegFourcc = 0x47504A4D; // "MJPG" in reverse order for little-endian
        CameraConfig leftConfig(bestResolution.width, bestResolution.height, static_cast<int>(bestFps), 4, mjpegFourcc, true);
        CameraConfig rightConfig(bestResolution.width, bestResolution.height, static_cast<int>(bestFps), 4, mjpegFourcc, true);

        // 使用QTimer异步添加相机，避免阻塞主线程
        QTimer::singleShot(0, [this, leftConfig, rightConfig]() {
            // 获取相机管理器单例
            MultiCameraManager& cameraManager = MultiCameraManager::instance();

            // 添加相机
            bool leftAdded = cameraManager.addCamera(m_leftCameraId.toStdString(), "左相机", leftConfig);
            bool rightAdded = cameraManager.addCamera(m_rightCameraId.toStdString(), "右相机", rightConfig);

            // 在主线程中更新UI
            QMetaObject::invokeMethod(this, [this, leftAdded, rightAdded]() {
                if (!leftAdded || !rightAdded) {
                    LOG_WARNING(QString("双相机模式部分初始化失败 - 左相机: %1, 右相机: %2")
                               .arg(leftAdded ? "成功" : "失败")
                               .arg(rightAdded ? "成功" : "失败"));

                    // 如果双相机模式失败，尝试单相机模式
                    if (leftAdded && !rightAdded) {
                        LOG_INFO("左相机成功，右相机失败，切换到单相机模式");
                        m_cameraMode = CameraMode::SINGLE_CAMERA;
                        m_rightCameraId = "";
                    } else if (!leftAdded && rightAdded) {
                        LOG_INFO("右相机成功，左相机失败，切换到单相机模式");
                        m_cameraMode = CameraMode::SINGLE_CAMERA;
                        m_leftCameraId = m_rightCameraId;
                        m_rightCameraId = "";
                    } else {
                        LOG_ERROR("双相机都初始化失败，进入无相机模式");
                        m_cameraMode = CameraMode::NO_CAMERA;
                        return;
                    }
                }

                // 设置同步模式
                MultiCameraManager& cameraManager = MultiCameraManager::instance();
                cameraManager.setSyncMode(SyncMode::NO_SYNC);

                m_camerasInitialized = true;
                LOG_INFO("双相机模式初始化完成");

                // 如果页面当前可见，则启用相机
                if (isVisible()) {
                    enableCameras();
                }
            }, Qt::QueuedConnection);
        });

        LOG_INFO("已发起双相机模式异步初始化请求");
    } catch (const std::exception& e) {
        LOG_ERROR(QString("双相机模式初始化时发生异常: %1").arg(e.what()));
        m_cameraMode = CameraMode::NO_CAMERA;
    } catch (...) {
        LOG_ERROR("双相机模式初始化时发生未知异常");
        m_cameraMode = CameraMode::NO_CAMERA;
    }
}

// 初始化单相机模式
void HomePage::initSingleCameraMode()
{
    LOG_INFO(QString("初始化单相机模式，使用设备: %1").arg(m_leftCameraId));

    try {
        // 获取相机支持的分辨率
        std::vector<cv::Size> resolutions = getSupportedResolutions(m_leftCameraId.toStdString());

        // 如果无法获取分辨率信息，使用默认值
        if (resolutions.empty()) {
            LOG_WARNING("无法获取相机分辨率信息，使用默认值: 1280x720");
            resolutions.push_back(cv::Size(1280, 720));
        }

        // 选择最佳分辨率 (优先选择1280x720)
        cv::Size bestResolution = {1280, 720}; // 默认值
        bool found = false;

        // 寻找1280x720分辨率
        for (const auto& res : resolutions) {
            if (res.width == 1280 && res.height == 720) {
                bestResolution = res;
                found = true;
                break;
            }
        }

        // 如果没有找到1280x720，选择最高分辨率
        if (!found && !resolutions.empty()) {
            bestResolution = resolutions[0];
            for (const auto& res : resolutions) {
                if (res.width * res.height > bestResolution.width * bestResolution.height) {
                    bestResolution = res;
                }
            }
        }

        LOG_INFO(QString("单相机模式选择的分辨率: %1x%2").arg(bestResolution.width).arg(bestResolution.height));

        // 获取支持的帧率
        std::vector<double> frameRates = getSupportedFrameRates(m_leftCameraId.toStdString(), bestResolution);

        // 如果无法获取帧率信息，使用默认值
        if (frameRates.empty()) {
            LOG_WARNING("无法获取相机帧率信息，使用默认值: 30fps");
            frameRates.push_back(30.0);
        }

        // 选择最佳帧率 (优先选择30fps)
        double bestFps = 30.0; // 默认值
        found = false;

        // 寻找30fps
        for (double rate : frameRates) {
            if (std::abs(rate - 30.0) < 1.0) {
                bestFps = 30.0;
                found = true;
                break;
            }
        }

        // 如果没有找到30fps，选择最高帧率
        if (!found && !frameRates.empty()) {
            bestFps = frameRates[0];
            for (double rate : frameRates) {
                if (rate > bestFps) {
                    bestFps = rate;
                }
            }
        }

        LOG_INFO(QString("单相机模式选择的帧率: %1").arg(bestFps));

        // 设置相机配置
        int mjpegFourcc = 0x47504A4D; // "MJPG" in reverse order for little-endian
        CameraConfig config(bestResolution.width, bestResolution.height, static_cast<int>(bestFps), 4, mjpegFourcc, true);

        // 使用QTimer异步添加相机，避免阻塞主线程
        QTimer::singleShot(0, [this, config]() {
            // 获取相机管理器单例
            MultiCameraManager& cameraManager = MultiCameraManager::instance();

            // 添加相机
            bool cameraAdded = cameraManager.addCamera(m_leftCameraId.toStdString(), "主相机", config);

            // 在主线程中更新UI
            QMetaObject::invokeMethod(this, [this, cameraAdded]() {
                if (!cameraAdded) {
                    LOG_ERROR("单相机模式初始化失败，进入无相机模式");
                    m_cameraMode = CameraMode::NO_CAMERA;
                    return;
                }

                // 设置同步模式
                MultiCameraManager& cameraManager = MultiCameraManager::instance();
                cameraManager.setSyncMode(SyncMode::NO_SYNC);

                m_camerasInitialized = true;
                LOG_INFO("单相机模式初始化完成");

                // 如果页面当前可见，则启用相机
                if (isVisible()) {
                    enableCameras();
                }
            }, Qt::QueuedConnection);
        });

        LOG_INFO("已发起单相机模式异步初始化请求");
    } catch (const std::exception& e) {
        LOG_ERROR(QString("单相机模式初始化时发生异常: %1").arg(e.what()));
        m_cameraMode = CameraMode::NO_CAMERA;
    } catch (...) {
        LOG_ERROR("单相机模式初始化时发生未知异常");
        m_cameraMode = CameraMode::NO_CAMERA;
    }
}

// 获取相机支持的分辨率列表
std::vector<cv::Size> HomePage::getSupportedResolutions(const std::string& cameraId) {
    std::vector<cv::Size> resolutions;
    cv::VideoCapture camera;
    
    LOG_DEBUG(QString("尝试获取相机支持的分辨率: %1").arg(QString::fromStdString(cameraId)));
    
    try {
        if (!camera.open(cameraId, cv::CAP_V4L2)) {
            LOG_WARNING(QString("无法打开相机获取分辨率信息: %1").arg(QString::fromStdString(cameraId)));
            return resolutions;
        }
        
        // 常见分辨率列表
        std::vector<cv::Size> commonResolutions = {
            {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, 
            {1920, 1080}, {2048, 1536}, {2560, 1440}, {3840, 2160}
        };
        
        // 获取当前分辨率作为参考
        int originalWidth = camera.get(cv::CAP_PROP_FRAME_WIDTH);
        int originalHeight = camera.get(cv::CAP_PROP_FRAME_HEIGHT);
        LOG_DEBUG(QString("相机当前分辨率: %1x%2").arg(originalWidth).arg(originalHeight));
        
        for (const auto& res : commonResolutions) {
            // 尝试设置分辨率
            camera.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
            camera.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);
            
            // 获取实际设置的分辨率
            int actualWidth = camera.get(cv::CAP_PROP_FRAME_WIDTH);
            int actualHeight = camera.get(cv::CAP_PROP_FRAME_HEIGHT);
            
            // 如果设置成功（实际值与请求值匹配），则添加到列表
            if (std::abs(actualWidth - res.width) < 10 && std::abs(actualHeight - res.height) < 10) {
                resolutions.push_back(cv::Size(actualWidth, actualHeight));
                LOG_DEBUG(QString("相机支持分辨率: %1x%2").arg(actualWidth).arg(actualHeight));
            }
        }
        
        // 恢复原始分辨率
        camera.set(cv::CAP_PROP_FRAME_WIDTH, originalWidth);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, originalHeight);
        
        // 关闭相机
        camera.release();
        
        LOG_INFO(QString("相机支持的分辨率数量: %1").arg(resolutions.size()));
        return resolutions;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("获取相机分辨率时发生异常: %1").arg(e.what()));
        if (camera.isOpened()) {
            camera.release();
        }
        return resolutions;
    } catch (...) {
        LOG_ERROR("获取相机分辨率时发生未知异常");
        if (camera.isOpened()) {
            camera.release();
        }
        return resolutions;
    }
}

// 获取相机支持的帧率
std::vector<double> HomePage::getSupportedFrameRates(const std::string& cameraId, const cv::Size& resolution) {
    std::vector<double> frameRates;
    cv::VideoCapture camera;
    
    LOG_DEBUG(QString("尝试获取相机支持的帧率: %1，分辨率: %2x%3")
             .arg(QString::fromStdString(cameraId))
             .arg(resolution.width)
             .arg(resolution.height));
    
    try {
        if (!camera.open(cameraId, cv::CAP_V4L2)) {
            LOG_WARNING(QString("无法打开相机获取帧率信息: %1").arg(QString::fromStdString(cameraId)));
            return frameRates;
        }
        
        // 设置分辨率
        camera.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);
        
        // 获取当前帧率作为参考
        double originalFps = camera.get(cv::CAP_PROP_FPS);
        LOG_DEBUG(QString("相机当前帧率: %1").arg(originalFps));
        
        // 常见帧率
        std::vector<double> commonFps = {15.0, 20.0, 24.0, 25.0, 30.0, 50.0, 60.0, 90.0, 120.0};
        
        for (double fps : commonFps) {
            // 尝试设置帧率
            camera.set(cv::CAP_PROP_FPS, fps);
            double actualFps = camera.get(cv::CAP_PROP_FPS);
            
            // 允许一定的误差
            if (std::abs(actualFps - fps) < 1.0) {
                frameRates.push_back(actualFps);
                LOG_DEBUG(QString("相机支持帧率: %1").arg(actualFps));
            }
        }
        
        // 恢复原始帧率
        camera.set(cv::CAP_PROP_FPS, originalFps);
        
        // 关闭相机
        camera.release();
        
        LOG_INFO(QString("相机支持的帧率数量: %1").arg(frameRates.size()));
        return frameRates;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("获取相机帧率时发生异常: %1").arg(e.what()));
        if (camera.isOpened()) {
            camera.release();
        }
        return frameRates;
    } catch (...) {
        LOG_ERROR("获取相机帧率时发生未知异常");
        if (camera.isOpened()) {
            camera.release();
        }
        return frameRates;
    }
}

void HomePage::initCameras()
{
    LOG_INFO("开始智能相机初始化...");

    // 初始化相机模式为无相机模式
    m_cameraMode = CameraMode::NO_CAMERA;

    try {
        // 调用智能相机检测
        smartCameraDetection();

    } catch (const std::exception& e) {
        LOG_ERROR(QString("智能相机初始化时发生异常: %1").arg(e.what()));
        m_cameraMode = CameraMode::NO_CAMERA;
    } catch (...) {
        LOG_ERROR("智能相机初始化时发生未知异常");
        m_cameraMode = CameraMode::NO_CAMERA;
    }
}

void HomePage::enableCameras()
{
    if (!m_camerasInitialized) {
        return;
    }
    
    LOG_INFO("主页启用相机...");
    
    try {
        // 获取相机管理器单例
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        
        // 确保先前的disableCameras已正确执行
        if (m_updateTimer && m_updateTimer->isActive()) {
            LOG_DEBUG("定时器已经在运行，先停止");
            m_updateTimer->stop();
        }
        
        LOG_DEBUG("主页增加引用计数...");
        
        // 增加引用计数
        int refCount = cameraManager.addReference(CLIENT_ID);
        
        LOG_DEBUG("主页引用计数增加完成，当前计数: " + QString::number(refCount));
        
        // 启动更新定时器，每50毫秒更新一次
        LOG_DEBUG("主页启动定时器...");
        m_updateTimer->start(50);
        
        LOG_INFO("主页相机启用完成");
    } catch (const std::exception& e) {
        LOG_ERROR(QString("主页启用相机时发生异常: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("主页启用相机时发生未知异常");
    }
}

void HomePage::disableCameras()
{
    if (!m_camerasInitialized) {
        return;
    }
    
    LOG_INFO("主页禁用相机...");
    
    try {
        // 停止更新定时器
        if (m_updateTimer) {
            LOG_DEBUG("主页停止定时器...");
            m_updateTimer->stop();
        }
        
        // 获取相机管理器单例
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        
        LOG_DEBUG("主页减少引用计数...");
        
        // 减少引用计数
        int refCount = cameraManager.removeReference(CLIENT_ID);
        
        LOG_DEBUG("主页引用计数减少完成，当前计数: " + QString::number(refCount));
        
        LOG_INFO("主页相机禁用完成");
    } catch (const std::exception& e) {
        LOG_ERROR(QString("主页禁用相机时发生异常: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("主页禁用相机时发生未知异常");
    }
}

/**
 * @brief 更新相机视图
 * 
 * 该方法负责更新主页上的相机视图。
 * 主视图和画中画(PIP)视图都显示左相机的画面，以便用户可以同时看到
 * 左相机的全局视图和局部细节。
 * 注意：虽然首页只显示左相机画面，但右相机仍然保持读流状态，以便在3D测量页面使用。
 */
void HomePage::updateCameraViews()
{
    // 避免不必要的调用
    static int logCounter = 0;
    
    if (!m_camerasInitialized || !isVisible() || !m_leftCameraView || !m_rightCameraView) {
        return;
    }
    
    try {
        // 增加计数器
        logCounter++;
        
        // 每100次调用记录一次日志，避免日志过多
        if (logCounter % 100 == 1) {
            LOG_DEBUG(QString("主页更新相机视图中...第%1次").arg(logCounter));
        }
        
        // 获取相机管理器单例
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        
        // 获取同步帧
        std::map<std::string, cv::Mat> frames;
        std::map<std::string, int64_t> timestamps;
        
        // 使用非阻塞方式获取同步帧
        if (!cameraManager.getSyncFrames(frames, timestamps, false)) {
            return;
        }
        
        // 遍历所有帧
        for (const auto& pair : frames) {
            const std::string& cameraId = pair.first;
            const cv::Mat& frame = pair.second;
            
            // 检查帧是否有效
            if (frame.empty()) {
                continue;
            }
            
            // 创建要显示的图像（默认使用原始帧）
            cv::Mat displayImage = frame.clone();
            
            // 应用图像滤镜处理（包括畸变校正等）
            displayImage = applyImageFilters(displayImage, cameraId);
            
            // 如果是左相机的帧且检测功能已启用，则处理检测逻辑
            if (cameraId == m_leftCameraId.toStdString() && m_detectionEnabled) {
                // 使用时间间隔控制检测频率，而不是帧数
                auto currentTime = std::chrono::steady_clock::now();
                auto timeSinceLastDetection = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - m_lastDetectionTime).count();
                
                // 每200ms最多提交一次检测请求
                if (timeSinceLastDetection >= DETECTION_INTERVAL_MS && !m_processingDetection.load()) {
                    m_lastDetectionTime = currentTime;
                    
                    // 异步提交检测请求，避免阻塞主线程
                    QTimer::singleShot(0, this, [this, frame, cameraId]() {
                        submitFrameForDetection(frame, cameraId);
                    });
                }
                
                // 始终使用当前最新帧作为显示基础，在上面叠加最近的检测结果
                displayImage = frame.clone(); // 确保使用当前最新帧
                
                // 非阻塞方式尝试获取最近的检测结果并叠加到当前帧
                if (m_detectionMutex.try_lock()) {
                    // 检查是否有有效的检测结果数据（不是完整图像，而是检测数据）
                    if (!m_lastDetectionResults.empty()) {
                        // 在当前最新帧上绘制最近的检测结果
                        drawDetectionResults(displayImage, m_lastDetectionResults);
                        
                        // 大幅减少日志频率
                        static int resultLogCounter = 0;
                        if (++resultLogCounter % 200 == 0) {
                            LOG_DEBUG(QString("在当前帧上绘制了 %1 个检测结果").arg(m_lastDetectionResults.size()));
                        }
                    }
                    m_detectionMutex.unlock();
                }
            }
                
            // 将OpenCV图像转换为QImage
            QImage qimg = matToQImage(displayImage);
                
            // 如果转换失败，跳过此帧
            if (qimg.isNull()) {
                continue;
            }
                
            // 根据相机ID更新对应的标签
            if (isVisible()) {
                // 更新左相机视图（主画面）
                if (cameraId == m_leftCameraId.toStdString() && m_leftCameraView && m_leftCameraView->isVisible()) {
                    QSize labelSize = m_leftCameraView->size();
                    if (labelSize.width() > 10 && labelSize.height() > 10) {
                        QImage toShow = qimg;
                        if (m_imageRotationDegrees != 0) {
                            QTransform t;
                            t.rotate(m_imageRotationDegrees);
                            toShow = qimg.transformed(t, Qt::SmoothTransformation);
                        }
                        if (m_flipHorizontal || m_flipVertical) {
                            toShow = toShow.mirrored(m_flipHorizontal, m_flipVertical);
                        }
                        if (m_invertColors) {
                            QImage tmp = toShow;
                            tmp.invertPixels();
                            toShow = tmp;
                        }

                        Qt::AspectRatioMode ratioMode = Qt::KeepAspectRatioByExpanding;
                        if (m_forceFitOnce || m_imageRotationDegrees == 90 || m_imageRotationDegrees == 270) {
                            ratioMode = Qt::KeepAspectRatio; // 保证完整显示
                        }
                        QSize scaledSize = labelSize * m_zoomScale;
                        m_leftCameraView->setPixmap(QPixmap::fromImage(toShow).scaled(
                            scaledSize, ratioMode, Qt::SmoothTransformation));
                        m_forceFitOnce = false;
                        
                        // 同时更新画中画（右相机视图），显示左相机的画面
                        if (m_rightCameraView && m_rightCameraView->isVisible()) {
                            QSize pipSize = m_rightCameraView->size();
                            if (pipSize.width() > 10 && pipSize.height() > 10) {
                                QImage pip = qimg;
                                if (m_imageRotationDegrees != 0) {
                                    QTransform t;
                                    t.rotate(m_imageRotationDegrees);
                                    pip = qimg.transformed(t, Qt::SmoothTransformation);
                                }
                                if (m_flipHorizontal || m_flipVertical) {
                                    pip = pip.mirrored(m_flipHorizontal, m_flipVertical);
                                }
                                if (m_invertColors) {
                                    QImage tmp = pip;
                                    tmp.invertPixels();
                                    pip = tmp;
                                }
                                QSize pipScaled = pipSize * std::max(0.5, std::min(2.0, m_zoomScale));
                                m_rightCameraView->setPixmap(QPixmap::fromImage(pip).scaled(
                                    pipScaled, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                            }
                        }
                    }
                }
                // 注意：这里不再显示右相机的画面，但右相机仍然保持读流状态
                // 右相机的帧会被获取并处理，只是不显示在UI上
            }
        }
        
        // 每秒更新一次帧率显示
        static auto lastFpsUpdateTime = std::chrono::steady_clock::now();
        auto currentTime = std::chrono::steady_clock::now();
        auto fpsElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFpsUpdateTime).count();
        
        if (fpsElapsed >= 1000) {
            // --- 添加/确保存在：获取 Camera Manager 实例 ---
            using namespace SmartScope::Core::CameraUtils;
            MultiCameraManager& cameraManager = MultiCameraManager::instance();
            // --- 添加结束 ---
            
            // 获取左右相机的帧率
            float leftFps = 0.0f;
            float rightFps = 0.0f;
            
            // 获取相机信息
            if (!m_leftCameraId.isEmpty()) {
                // 确保这里使用的是 cameraManager
                auto leftCameraInfo = cameraManager.getCameraInfo(m_leftCameraId.toStdString());
                leftFps = leftCameraInfo.fps;
            }
            
            if (!m_rightCameraId.isEmpty()) {
                 // 确保这里使用的是 cameraManager
                auto rightCameraInfo = cameraManager.getCameraInfo(m_rightCameraId.toStdString());
                rightFps = rightCameraInfo.fps;
            }
            
            // 更新状态栏中的帧率显示
            updateStatusBarFps(leftFps, rightFps);
            
            lastFpsUpdateTime = currentTime;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("主页更新相机视图时发生异常: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("主页更新相机视图时发生未知异常");
    }
}

QImage HomePage::matToQImage(const cv::Mat &mat)
{
    try {
        // 检查图像是否为空
        if (mat.empty()) {
            return QImage();
        }
        
        // 检查图像尺寸
        if (mat.rows <= 0 || mat.cols <= 0) {
            return QImage();
        }
        
        // 检查数据指针
        if (!mat.data) {
            return QImage();
        }
        
        // 转换为RGB格式（如果需要）
        cv::Mat rgb;
        
        try {
            // 根据通道数选择合适的转换方式
            switch (mat.channels()) {
                case 1:
                    cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
                    break;
                case 3:
                    // 检查连续性，如果不连续则复制
                    if (mat.isContinuous()) {
                        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
                    } else {
                        cv::cvtColor(mat.clone(), rgb, cv::COLOR_BGR2RGB);
                    }
                    break;
                case 4:
                    // 检查连续性，如果不连续则复制
                    if (mat.isContinuous()) {
                        cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGB);
                    } else {
                        cv::cvtColor(mat.clone(), rgb, cv::COLOR_BGRA2RGB);
                    }
                    break;
                default:
                    return QImage();
            }
        } catch (const cv::Exception& e) {
            return QImage();
        }
        
        // 检查转换后的图像
        if (rgb.empty() || !rgb.data) {
            return QImage();
        }
        
        // 确保图像数据是连续的
        if (!rgb.isContinuous()) {
            rgb = rgb.clone();
        }
        
        // 创建QImage - 使用深拷贝确保安全
        QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        return qimg.copy(); // 深拷贝，确保数据不会被释放
    } catch (const std::exception& e) {
        LOG_ERROR(QString("转换图像时发生异常: %1").arg(e.what()));
        return QImage();
    } catch (...) {
        LOG_ERROR("转换图像时发生未知异常");
        return QImage();
    }
}

bool HomePage::eventFilter(QObject *obj, QEvent *event)
{
    // 处理相机标签的事件
    if (obj == m_leftCameraView || obj == m_rightCameraView) {
        // --- 修改：移除点击切换逻辑 ---
        // if (event->type() == QEvent::MouseButtonPress) {
        //     QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        //     if (mouseEvent->button() == Qt::LeftButton) {
        //         // 交换相机显示 (整段逻辑移除或注释掉)
        //         // ...
        //         return true; 
        //     }
        // }
        // --- 修改结束 ---
    }
    
    // 处理鼠标点击事件：与高级设置一致，点击面板外关闭
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // 获取点击的全局坐标
        QPoint globalPos = mouseEvent->globalPos();
        
        // 高级设置面板
        if (m_adjustmentPanelVisible && m_adjustmentPanel) {
            QRect panelGeometry = QRect(m_adjustmentPanel->mapToGlobal(QPoint(0, 0)), m_adjustmentPanel->size());
            if (!panelGeometry.contains(globalPos)) {
                QWidget* mainWindow = this->window();
                if (mainWindow) {
                    ToolBar* toolBar = mainWindow->findChild<ToolBar*>();
                    if (toolBar) {
                        QPushButton* adjustButton = toolBar->getButton("adjustButton");
                        if (adjustButton) {
                            QRect buttonGeometry = QRect(adjustButton->mapToGlobal(QPoint(0, 0)), adjustButton->size());
                            if (buttonGeometry.contains(globalPos)) {
                                return false;
                            }
                        }
                    }
                }
                LOG_INFO("检测到点击调节面板外部区域，自动隐藏面板");
                QTimer::singleShot(0, this, &HomePage::toggleAdjustmentPanel);
            }
        }

        // RGA面板
        if (m_rgaPanelVisible && m_rgaPanel) {
            QRect rgaGeo = QRect(m_rgaPanel->mapToGlobal(QPoint(0, 0)), m_rgaPanel->size());
            if (!rgaGeo.contains(globalPos)) {
                QWidget* mainWindow = this->window();
                if (mainWindow) {
                    ToolBar* toolBar = mainWindow->findChild<ToolBar*>();
                    if (toolBar) {
                        QPushButton* adjustButton = toolBar->getButton("adjustButton");
                        if (adjustButton) {
                            QRect buttonGeometry = QRect(adjustButton->mapToGlobal(QPoint(0, 0)), adjustButton->size());
                            if (buttonGeometry.contains(globalPos)) {
                                return false;
                            }
                        }
                    }
                }
                LOG_INFO("检测到点击RGA面板外部区域，自动隐藏");
                QTimer::singleShot(0, this, &HomePage::toggleRgaPanel);
            }
        }
    }
    
    // 处理画中画拖动
    if (obj == m_rightCameraView) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragStartPosition = mouseEvent->pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                // 计算拖动距离
                QPoint delta = mouseEvent->pos() - m_dragStartPosition;
                // 移动画中画
                m_rightCameraView->move(m_rightCameraView->pos() + delta);
                return true;
            }
        }
    }
    
    // 其他情况调用基类方法
    return BasePage::eventFilter(obj, event);
}

void HomePage::resizeEvent(QResizeEvent *event)
{
    BasePage::resizeEvent(event);
    
    // 更新相机标签位置
    updateCameraPositions();
    
    // 更新调节面板位置
    if (m_adjustmentPanel) {
        QWidget* mainWindow = this->window();
        int panelWidth = 550;
        int rightMargin = 150;  // 与createAdjustmentPanel()方法中保持一致
        
        // 计算面板在主窗口中的位置
        QPoint contentPos = m_contentWidget->mapToGlobal(QPoint(0, 0));
        QPoint mainWindowPos = mainWindow->mapFromGlobal(contentPos);
        int panelX = mainWindowPos.x() + m_contentWidget->width() - panelWidth - rightMargin;
        int panelY = mainWindowPos.y() + 80;  // 从60px增加到80px，适应更高的状态栏
        
        // 设置面板位置
        m_adjustmentPanel->setGeometry(panelX, panelY, panelWidth, 800);
    }
    
    // 窗口大小变化时，确保画中画位置合适
    if (m_rightCameraView && m_rightCameraView->isVisible()) {
        // 获取当前画中画位置和大小
        QPoint currentPos = m_rightCameraView->pos();
        QSize currentSize = m_rightCameraView->size();
        
        // 调整画中画位置，确保不超出窗口边界
        adjustPipView(currentPos, currentSize);
    }
}

void HomePage::showEvent(QShowEvent *event)
{
    LOG_INFO("主页显示事件开始");
    
    // 先调用基类方法
    BasePage::showEvent(event);
    
    // 检查并尝试获取 PathSelector
    QTimer::singleShot(100, this, [this]() {
        // 如果 m_pathSelector 为空，尝试从状态栏获取
        if (!m_pathSelector) {
            QWidget* mainWindow = this->window();
            if (mainWindow) {
                // 查找主窗口中的状态栏
                QList<SmartScope::StatusBar*> statusBars = mainWindow->findChildren<SmartScope::StatusBar*>();
                if (!statusBars.isEmpty()) {
                    // 获取第一个状态栏中的路径选择器
                    SmartScope::StatusBar* statusBar = statusBars.first();
                    SmartScope::PathSelector* pathSelector = statusBar->getPathSelector();
                    
                    if (pathSelector) {
                        // 设置路径选择器的当前路径
                        pathSelector->setCurrentPath(m_currentWorkPath);
                        
                        // 连接路径选择器的信号
                        connect(pathSelector, &SmartScope::PathSelector::pathChanged, this, &HomePage::onWorkPathChanged);
                        
                        LOG_INFO("在显示事件中连接状态栏的路径选择器");
                        m_pathSelector = pathSelector;
                    }
                }
            }
        }
        
        // 如果找到了路径选择器，确保它显示在正确位置
        if (m_pathSelector) {
            m_pathSelector->show();
            m_pathSelector->raise();
        } else {
            LOG_WARNING("显示事件中未找到路径选择器");
        }
    });
    
    // 初始化工具栏按钮
    QTimer::singleShot(300, this, [this]() {
        initToolBarButtons();
    });
    
    // 在事件处理后启用相机 - 使用单次定时器避免在事件处理期间启用相机
    QTimer::singleShot(100, this, [this]() {
        LOG_DEBUG("主页延迟启用相机");
        enableCameras();
    });
    
    LOG_INFO("主页显示事件结束");
}

void HomePage::hideEvent(QHideEvent *event)
{
    LOG_INFO("主页隐藏事件开始");
    
    // 如果调节面板可见，则隐藏它
    if (m_adjustmentPanelVisible && m_adjustmentPanel) {
        LOG_INFO("页面切换时关闭调节面板");
        m_adjustmentPanel->hide();
        m_adjustmentPanelVisible = false;
    }
    
    // 先禁用相机
    disableCameras();
    
    // 然后调用基类方法
    BasePage::hideEvent(event);
    
    LOG_INFO("主页隐藏事件结束");
}

void HomePage::updateCameraPositions()
{
    if (!m_rightCameraView) {
        return;
    }
    
    // 将右相机标签移动到左上角，距离边缘20像素，垂直位置从70px改为90px以适应更高的状态栏
    m_rightCameraView->move(20, 90);
}

void HomePage::createAdjustmentPanel()
{
    LOG_INFO("创建调节面板...");
    
    // 创建控制区域 - 修改为直接使用主窗口，而不是内容部件
    QWidget* mainWindow = this->window();
    m_adjustmentPanel = new QWidget(mainWindow);
    m_adjustmentPanel->setObjectName("adjustmentPanel");
    
    // 确保控件可以接收鼠标事件
    m_adjustmentPanel->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_adjustmentPanel->setFocusPolicy(Qt::StrongFocus);
    m_adjustmentPanel->setMouseTracking(true);
    
    // 为主窗口安装事件过滤器，以便捕获所有点击事件
    mainWindow->installEventFilter(this);
    
    m_adjustmentPanel->setStyleSheet(
        "QWidget#adjustmentPanel {"
        "   background-color: rgba(30, 30, 30, 240);"  // 增加不透明度
        "   border: 2px solid #444444;"  // 更明显的边框
        "   border-radius: 10px;"
        "}"
        "QLabel {"
        "   color: white;"
        "   font-size: 22px;"
        "   font-weight: bold;"
        "}"
        "QSlider {"
        "   height: 50px;"
        "}"
        "QSlider::groove:horizontal {"
        "   height: 14px;"
        "   background: #555;"
        "   border-radius: 7px;"
        "}"
        "QSlider::handle:horizontal {"
        "   width: 36px;"
        "   height: 36px;"
        "   margin: -12px 0;"
        "   background: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
        "                              stop:0 #888888, stop:0.8 #888888, stop:1 #555555);"
        "   border-radius: 18px;"
        "   border: 2px solid #000000;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, "
        "                              stop:0 #AAAAAA, stop:0.8 #AAAAAA, stop:1 #777777);"
        "}"
        "QPushButton {"
        "   background-color: #555555;"
        "   color: white;"
        "   border-radius: 15px;"
        "   padding: 15px 25px;"
        "   font-size: 22px;"
        "   font-weight: bold;"
        "   border: 2px solid #000000;"
        "}"
        "QPushButton:hover {"
        "   background-color: #777777;"
        "   border: 2px solid #333333;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #333333;"
        "   border: 2px solid #000000;"
        "}"
        "QCheckBox {"
        "   color: white;"
        "   font-size: 22px;"
        "   spacing: 15px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 30px;"
        "   height: 30px;"
        "   border-radius: 4px;"
        "   border: 2px solid #000000;"
        "}"
        "QCheckBox::indicator:checked {"
        "   background-color: #555555;"
        "   image: url(:/icons/check.svg);"
        "}"
    );
    
    // 设置控制区域大小
    int panelWidth = 550;
    int panelHeight = 800;
    int rightMargin = 150;  // 减小右边距，使面板向右移动
    
    // 计算面板在主窗口中的位置
    QPoint contentPos = m_contentWidget->mapToGlobal(QPoint(0, 0));
    QPoint mainWindowPos = mainWindow->mapFromGlobal(contentPos);
    int panelX = mainWindowPos.x() + m_contentWidget->width() - panelWidth - rightMargin;
    int panelY = mainWindowPos.y() + 80;  // 从60px增加到80px，适应更高的状态栏
    
    // 设置面板位置
    m_adjustmentPanel->setGeometry(panelX, panelY, panelWidth, panelHeight);
    
    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(25);  // 增大表单项间距
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    
    // 自动曝光复选框
    QCheckBox *autoExposureCheck = new QCheckBox("自动曝光");
    autoExposureCheck->setStyleSheet("color: white; font-size: 22px;");
    autoExposureCheck->setChecked(true);
    formLayout->addRow("", autoExposureCheck);
    m_checkBoxes["auto_exposure"] = autoExposureCheck;
    
    // 曝光时间滑块
    QSlider *exposureSlider = createSlider(3, 2047, 3);
    QLabel *exposureLabel = createLabel("曝光时间:");
    QLabel *exposureValueLabel = createLabel("3");
    exposureValueLabel->setFixedWidth(60);
    exposureValueLabel->setAlignment(Qt::AlignCenter);
    connect(exposureSlider, &QSlider::valueChanged, [exposureValueLabel](int value) {
        exposureValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *exposureLayout = new QHBoxLayout();
    exposureLayout->addWidget(exposureSlider);
    exposureLayout->addWidget(exposureValueLabel);
    formLayout->addRow(exposureLabel, exposureLayout);
    m_sliders["exposure_time_absolute"] = exposureSlider;
    
    // 连接自动曝光复选框状态变化信号
    connect(autoExposureCheck, &QCheckBox::stateChanged, [this, exposureSlider](int state) {
        bool enabled = (state != Qt::Checked);
        exposureSlider->setEnabled(enabled);
        LOG_DEBUG(QString("自动曝光状态变化，设置曝光时间滑块状态为: %1").arg(enabled ? "启用" : "禁用"));
    });
    
    // 确保初始状态时曝光时间滑块的启用状态与自动曝光复选框状态一致
    bool autoExposureEnabled = autoExposureCheck->isChecked();
    exposureSlider->setEnabled(!autoExposureEnabled);
    
    // 自动白平衡复选框
    QCheckBox *autoWhiteBalanceCheck = new QCheckBox("自动白平衡");
    autoWhiteBalanceCheck->setStyleSheet("color: white; font-size: 22px;");
    autoWhiteBalanceCheck->setChecked(false);
    formLayout->addRow("", autoWhiteBalanceCheck);
    m_checkBoxes["white_balance_auto_preset"] = autoWhiteBalanceCheck;
    
    // 手动白平衡滑块
    QSlider *whiteBalanceSlider = createSlider(2000, 6500, 4500);
    QLabel *whiteBalanceLabel = createLabel("白平衡温度:");
    QLabel *whiteBalanceValueLabel = createLabel("4500");
    whiteBalanceValueLabel->setFixedWidth(60);
    whiteBalanceValueLabel->setAlignment(Qt::AlignCenter);
    connect(whiteBalanceSlider, &QSlider::valueChanged, [whiteBalanceValueLabel](int value) {
        whiteBalanceValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *whiteBalanceLayout = new QHBoxLayout();
    whiteBalanceLayout->addWidget(whiteBalanceSlider);
    whiteBalanceLayout->addWidget(whiteBalanceValueLabel);
    formLayout->addRow(whiteBalanceLabel, whiteBalanceLayout);
    m_sliders["white_balance_temperature"] = whiteBalanceSlider;
    
    // 连接自动白平衡复选框状态变化信号
    connect(autoWhiteBalanceCheck, &QCheckBox::stateChanged, [this, whiteBalanceSlider](int state) {
        bool enabled = (state != Qt::Checked);
        whiteBalanceSlider->setEnabled(enabled);
        LOG_DEBUG(QString("自动白平衡状态变化，设置白平衡温度滑块状态为: %1").arg(enabled ? "启用" : "禁用"));
    });
    
    // 确保初始状态时白平衡温度滑块的启用状态与自动白平衡复选框状态一致
    bool autoWhiteBalanceEnabled = autoWhiteBalanceCheck->isChecked();
    whiteBalanceSlider->setEnabled(!autoWhiteBalanceEnabled);
    
    // 亮度滑块
    QSlider *brightnessSlider = createSlider(-64, 64, 0);
    QLabel *brightnessLabel = createLabel("亮度:");
    QLabel *brightnessValueLabel = createLabel("0");
    brightnessValueLabel->setFixedWidth(60);
    brightnessValueLabel->setAlignment(Qt::AlignCenter);
    connect(brightnessSlider, &QSlider::valueChanged, [brightnessValueLabel](int value) {
        brightnessValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *brightnessLayout = new QHBoxLayout();
    brightnessLayout->addWidget(brightnessSlider);
    brightnessLayout->addWidget(brightnessValueLabel);
    formLayout->addRow(brightnessLabel, brightnessLayout);
    m_sliders["brightness"] = brightnessSlider;
    
    // 对比度滑块
    QSlider *contrastSlider = createSlider(0, 95, 0);
    QLabel *contrastLabel = createLabel("对比度:");
    QLabel *contrastValueLabel = createLabel("0");
    contrastValueLabel->setFixedWidth(60);
    contrastValueLabel->setAlignment(Qt::AlignCenter);
    connect(contrastSlider, &QSlider::valueChanged, [contrastValueLabel](int value) {
        contrastValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(contrastSlider);
    contrastLayout->addWidget(contrastValueLabel);
    formLayout->addRow(contrastLabel, contrastLayout);
    m_sliders["contrast"] = contrastSlider;
    
    // 饱和度滑块
    QSlider *saturationSlider = createSlider(0, 100, 50);
    QLabel *saturationLabel = createLabel("饱和度:");
    QLabel *saturationValueLabel = createLabel("50");
    saturationValueLabel->setFixedWidth(60);
    saturationValueLabel->setAlignment(Qt::AlignCenter);
    connect(saturationSlider, &QSlider::valueChanged, [saturationValueLabel](int value) {
        saturationValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *saturationLayout = new QHBoxLayout();
    saturationLayout->addWidget(saturationSlider);
    saturationLayout->addWidget(saturationValueLabel);
    formLayout->addRow(saturationLabel, saturationLayout);
    m_sliders["saturation"] = saturationSlider;
    
    // 背光补偿滑块
    QSlider *backlightSlider = createSlider(0, 8, 0);
    QLabel *backlightLabel = createLabel("背光补偿:");
    QLabel *backlightValueLabel = createLabel("0");
    backlightValueLabel->setFixedWidth(60);
    backlightValueLabel->setAlignment(Qt::AlignCenter);
    connect(backlightSlider, &QSlider::valueChanged, [backlightValueLabel](int value) {
        backlightValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *backlightLayout = new QHBoxLayout();
    backlightLayout->addWidget(backlightSlider);
    backlightLayout->addWidget(backlightValueLabel);
    formLayout->addRow(backlightLabel, backlightLayout);
    m_sliders["backlight_compensation"] = backlightSlider;
    
    // Gamma滑块
    QSlider *gammaSlider = createSlider(32, 300, 100);
    QLabel *gammaLabel = createLabel("Gamma:");
    QLabel *gammaValueLabel = createLabel("100");
    gammaValueLabel->setFixedWidth(60);
    gammaValueLabel->setAlignment(Qt::AlignCenter);
    connect(gammaSlider, &QSlider::valueChanged, [gammaValueLabel](int value) {
        gammaValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *gammaLayout = new QHBoxLayout();
    gammaLayout->addWidget(gammaSlider);
    gammaLayout->addWidget(gammaValueLabel);
    formLayout->addRow(gammaLabel, gammaLayout);
    m_sliders["gamma"] = gammaSlider;
    
    // 增益滑块
    QSlider *gainSlider = createSlider(0, 3, 0);
    QLabel *gainLabel = createLabel("增益:");
    QLabel *gainValueLabel = createLabel("0");
    gainValueLabel->setFixedWidth(60);
    gainValueLabel->setAlignment(Qt::AlignCenter);
    connect(gainSlider, &QSlider::valueChanged, [gainValueLabel](int value) {
        gainValueLabel->setText(QString::number(value));
    });
    QHBoxLayout *gainLayout = new QHBoxLayout();
    gainLayout->addWidget(gainSlider);
    gainLayout->addWidget(gainValueLabel);
    formLayout->addRow(gainLabel, gainLayout);
    m_sliders["gain"] = gainSlider;
    
    // 添加按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(30);
    buttonLayout->setAlignment(Qt::AlignCenter);
    
    // 重置按钮
    QPushButton *resetButton = new QPushButton("重置默认值");
    resetButton->setMinimumHeight(60);
    resetButton->setMinimumWidth(180);
    connect(resetButton, &QPushButton::clicked, this, &HomePage::resetToDefaults);
    buttonLayout->addWidget(resetButton);
    
    // 应用按钮
    QPushButton *applyButton = new QPushButton("应用设置");
    applyButton->setMinimumHeight(60);
    connect(applyButton, &QPushButton::clicked, this, &HomePage::applySettings);
    buttonLayout->addWidget(applyButton);
    
    // 设置控制区域布局
    QVBoxLayout *controlLayout = new QVBoxLayout(m_adjustmentPanel);
    controlLayout->setContentsMargins(40, 40, 40, 40);
    
    // 先添加表单布局
    controlLayout->addLayout(formLayout);
    
    // 添加按钮布局，设置上边距
    controlLayout->addSpacing(20);
    controlLayout->addLayout(buttonLayout);
    
    // 默认隐藏调节面板
    m_adjustmentPanel->hide();
    m_adjustmentPanelVisible = false;
    
    LOG_INFO("调节面板创建完成");
}

void HomePage::createRgaPanel()
{
    if (m_rgaPanel) return;

    QWidget* mainWindow = this->window();
    m_rgaPanel = new QWidget(mainWindow);
    m_rgaPanel->setObjectName("rgaPanel");
    m_rgaPanel->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_rgaPanel->setFocusPolicy(Qt::StrongFocus);
    m_rgaPanel->setMouseTracking(true);

    m_rgaPanel->setStyleSheet(
        "QWidget#rgaPanel {"
        "   background-color: rgba(24, 24, 24, 120);"
        "   border: 1px solid #2E2E2E;"
        "   border-radius: 14px;"
        "}"
    );

    // 阴影效果，贴近返回弹窗风格
    auto *shadow = new QGraphicsDropShadowEffect(m_rgaPanel);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 160));
    m_rgaPanel->setGraphicsEffect(shadow);

    QVBoxLayout* layout = new QVBoxLayout(m_rgaPanel);
    layout->setContentsMargins(16, 10, 16, 12);
    layout->setSpacing(10);

    QLabel* title = new QLabel("画面调整", m_rgaPanel);
    title->setStyleSheet("color: #FFFFFF; font-size: 32px; font-weight: 700; letter-spacing: 0.5px; padding: 4px 0;");
    title->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title);

    // 功能按钮网格（3列x2行），按钮使用QToolButton，图标在上文字在下
    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(18);
    grid->setContentsMargins(20, 6, 20, 6);
    auto mkBtn = [this](const QString& text){
        QToolButton* b = new QToolButton(m_rgaPanel);
        b->setText(text);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(65, 65));
        b->setMinimumSize(200, 170);
        b->setAutoRaise(false);
        b->setStyleSheet(
            "QToolButton {"
            "  font-size: 26px; font-weight: 700; color: #EEEEEE;"
            "  background-color: rgba(58, 58, 58, 130); border: 1px solid rgba(74, 74, 74, 160); border-radius: 12px;"
            "  padding: 10px 12px;"
            "}"
            "QToolButton:hover { background-color: rgba(74, 74, 74, 150); border: 1px solid rgba(90, 90, 90, 170); }"
            "QToolButton:pressed { background-color: rgba(47, 47, 47, 120); }"
        );
        return b;
    };

    QToolButton* btnRot90   = mkBtn("旋转");
    QToolButton* btnFlipH   = mkBtn("水平翻转");
    QToolButton* btnFlipV   = mkBtn("垂直翻转");
    QToolButton* btnInvert  = mkBtn("反色");
    QToolButton* btnExpoCycle = mkBtn("自动曝光");
    QToolButton* btnBrightness = mkBtn("亮度");
    QToolButton* btnDistortion = mkBtn("畸变校正");
    QToolButton* btnReset = mkBtn("还原");
    QToolButton* btnAdvanced = mkBtn("高级模式");

    // 使用自定义SVG图标
    btnRot90->setIcon(QIcon(":/icons/rotate.svg"));
    btnFlipH->setIcon(QIcon(":/icons/horizontal_filp.svg"));
    btnFlipV->setIcon(QIcon(":/icons/vertical_filp.svg"));
    btnInvert->setIcon(QIcon(":/icons/invert_color.svg"));
    btnBrightness->setIcon(QIcon(":/icons/brightness.svg"));
    btnDistortion->setIcon(QIcon(":/icons/distortion.svg"));
    btnReset->setIcon(QIcon(":/icons/restore.svg"));
    btnAdvanced->setIcon(QIcon(":/icons/advanced_settings.svg"));
    // 曝光按钮根据状态设置图标
    btnExpoCycle->setIcon(QIcon(":/icons/auto_exposure.svg"));

    // 3x3网格布局，添加畸变校正按钮
    grid->addWidget(btnRot90,      0, 0);
    grid->addWidget(btnFlipH,      0, 1);
    grid->addWidget(btnFlipV,      0, 2);
    grid->addWidget(btnInvert,     1, 0);
    grid->addWidget(btnExpoCycle,  1, 1);
    grid->addWidget(btnBrightness, 1, 2);
    grid->addWidget(btnDistortion, 2, 0);
    grid->addWidget(btnReset,      2, 1);
    grid->addWidget(btnAdvanced,   2, 2);
    layout->addLayout(grid);
    connect(btnRot90, &QToolButton::clicked, this, [this]() {
        m_imageRotationDegrees = (m_imageRotationDegrees + 90) % 360;
        // 对于90/270强制下一帧完整适配显示
        if (m_imageRotationDegrees == 90 || m_imageRotationDegrees == 270) {
            m_forceFitOnce = true;
        }
    });

    connect(btnFlipH, &QToolButton::clicked, this, [this]() {
        // 通过缩放-1实现水平翻转，统一在渲染时应用
        m_flipHorizontal = !m_flipHorizontal;
    });
    connect(btnFlipV, &QToolButton::clicked, this, [this]() {
        m_flipVertical = !m_flipVertical;
    });
    connect(btnInvert, &QToolButton::clicked, this, [this]() {
        m_invertColors = !m_invertColors;
    });
    
    // 畸变校正按钮（点击后不打勾，仅切换功能）
    connect(btnDistortion, &QToolButton::clicked, this, [this, btnDistortion]() {
        toggleDistortionCorrection();
        btnDistortion->setText("畸变校正");
    });

    // 亮度按钮：调用 LedController，与工具栏一致
    auto updateBrightnessText = [btnBrightness]() {
        if (LedController::instance().isConnected()) {
            int percent = LedController::instance().getCurrentBrightnessPercentage();
            btnBrightness->setText(QString("亮度 %1%").arg(percent));
        } else {
            btnBrightness->setText("亮度 未连接");
        }
    };
    updateBrightnessText();
    connect(btnBrightness, &QToolButton::clicked, this, [this, updateBrightnessText]() mutable {
        if (!LedController::instance().isConnected()) {
            showToast(this, "LED未连接", 1500);
            return;
        }
        if (LedController::instance().toggleBrightness()) {
            updateBrightnessText();
        } else {
            showToast(this, "亮度切换失败", 1500);
        }
    });

    // 单按钮循环：自动曝光 -> 50 -> 100 -> 300 -> 500 -> 1000 -> 1500 -> 自动曝光
    static const int kExposurePresets[] = {50, 100, 300, 500, 1000, 1500};
    static constexpr int kExposurePresetCount = sizeof(kExposurePresets)/sizeof(kExposurePresets[0]);

    // 从高级设置同步当前状态作为初始显示
    bool autoFromUi = m_checkBoxes.contains("auto_exposure") ? m_checkBoxes["auto_exposure"]->isChecked() : m_autoExposureEnabledRga;
    int currExpo = m_sliders.contains("exposure_time_absolute") ? m_sliders["exposure_time_absolute"]->value() : kExposurePresets[0];
    // 找到当前曝光值对应的预设索引（不严格，按最近匹配）
    int nearestIdx = 0; int nearestDiff = std::abs(currExpo - kExposurePresets[0]);
    for (int i = 1; i < kExposurePresetCount; ++i) {
        int d = std::abs(currExpo - kExposurePresets[i]);
        if (d < nearestDiff) { nearestDiff = d; nearestIdx = i; }
    }
    m_exposurePresetIndex = nearestIdx;
    m_autoExposureEnabledRga = autoFromUi;

    // 初始化按钮文字和图标（依据当前状态）
    btnExpoCycle->setText(m_autoExposureEnabledRga ? "自动曝光"
                                                   : QString("曝光 %1").arg(kExposurePresets[m_exposurePresetIndex]));
    btnExpoCycle->setIcon(QIcon(m_autoExposureEnabledRga ? ":/icons/auto_exposure.svg" : ":/icons/exposure.svg"));

    connect(btnExpoCycle, &QPushButton::clicked, this, [this, btnExpoCycle]() {
        if (m_autoExposureEnabledRga) {
            // 自动 -> 手动的第一档固定为50（索引0）
            m_autoExposureEnabledRga = false;
            m_exposurePresetIndex = 0;
            int val = kExposurePresets[m_exposurePresetIndex];
            btnExpoCycle->setText(QString("曝光 %1").arg(val));
            btnExpoCycle->setIcon(QIcon(":/icons/exposure.svg"));
            if (m_checkBoxes.contains("auto_exposure")) m_checkBoxes["auto_exposure"]->setChecked(false);
            if (m_sliders.contains("exposure_time_absolute")) m_sliders["exposure_time_absolute"]->setValue(val);
            QMap<QString, QString> params; params["auto_exposure"] = "1"; params["exposure_time_absolute"] = QString::number(val);
            applyParamsToCamera(m_leftCameraId, params);
            return;
        }

        // 手动：下一档；超过1500则回到自动
        m_exposurePresetIndex++;
        if (m_exposurePresetIndex >= kExposurePresetCount) {
            m_autoExposureEnabledRga = true;
            btnExpoCycle->setText("自动曝光");
            btnExpoCycle->setIcon(QIcon(":/icons/auto_exposure.svg"));
            if (m_checkBoxes.contains("auto_exposure")) m_checkBoxes["auto_exposure"]->setChecked(true);
            QMap<QString, QString> params; params["auto_exposure"] = "3";
            applyParamsToCamera(m_leftCameraId, params);
            // 不改变 m_exposurePresetIndex，这样下次从自动再转手动时仍从50开始
        } else {
            int val = kExposurePresets[m_exposurePresetIndex];
            btnExpoCycle->setText(QString("曝光 %1").arg(val));
            btnExpoCycle->setIcon(QIcon(":/icons/exposure.svg"));
            if (m_sliders.contains("exposure_time_absolute")) m_sliders["exposure_time_absolute"]->setValue(val);
            QMap<QString, QString> params; params["auto_exposure"] = "1"; params["exposure_time_absolute"] = QString::number(val);
            applyParamsToCamera(m_leftCameraId, params);
        }
    });

    connect(btnAdvanced, &QToolButton::clicked, this, [this]() {
        if (m_rgaPanel) m_rgaPanel->hide();
        m_rgaPanelVisible = false;
        toggleAdjustmentPanel();
    });

    // 还原逻辑
    connect(btnReset, &QToolButton::clicked, this, [this, btnExpoCycle]() {
        m_imageRotationDegrees = 0;
        m_flipHorizontal = false;
        m_flipVertical = false;
        m_invertColors = false;
        m_zoomScale = 1.0;
        m_forceFitOnce = true;
        // 曝光回到自动
        m_autoExposureEnabledRga = true;
        m_exposurePresetIndex = 0;
        btnExpoCycle->setText("自动曝光");
        btnExpoCycle->setIcon(QIcon(":/icons/auto_exposure.svg"));
        if (m_checkBoxes.contains("auto_exposure")) {
            m_checkBoxes["auto_exposure"]->setChecked(true);
        }
        QMap<QString, QString> params;
        params["auto_exposure"] = "3";
        applyParamsToCamera(m_leftCameraId, params);
    });

    // 面板居中且更大
    QWidget* mainWnd = this->window();
    QSize screenSize = mainWnd ? mainWnd->size() : QSize(1280, 800);
    int panelWidth = std::max(720, static_cast<int>(screenSize.width() * 0.7));
    int panelHeight = std::max(480, static_cast<int>(screenSize.height() * 0.6));
    int panelX = (screenSize.width() - panelWidth) / 2;
    int panelY = (screenSize.height() - panelHeight) / 2;
    m_rgaPanel->setGeometry(panelX, panelY, panelWidth, panelHeight);

    m_rgaPanel->hide();
    m_rgaPanelVisible = false;
}

void HomePage::toggleRgaPanel()
{
    if (!m_rgaPanel) {
        createRgaPanel();
    }

    if (!m_rgaPanelVisible) {
        if (m_adjustmentPanel && m_adjustmentPanel->isVisible()) {
            m_adjustmentPanel->hide();
            m_adjustmentPanelVisible = false;
        }
        m_rgaPanel->raise();
        m_rgaPanel->show();
        m_rgaPanelVisible = true;
    } else {
        m_rgaPanel->hide();
        m_rgaPanelVisible = false;
    }
}

void HomePage::toggleAdjustmentPanel()
{
    LOG_INFO("切换调节面板可见性，当前状态: " + QString(m_adjustmentPanelVisible ? "可见" : "隐藏"));
    
    // 检查控件是否存在
    if (!m_adjustmentPanel) {
        LOG_ERROR("调节面板控件不存在，无法切换可见性");
        showToast(this, "调节面板控件不存在，请重启应用", 2000);
        return;
    }
    
    m_adjustmentPanelVisible = !m_adjustmentPanelVisible;
    if (m_adjustmentPanelVisible) {
        // 确保面板在正确的位置
        QWidget* mainWindow = this->window();
        if (!mainWindow) {
            LOG_ERROR("无法获取主窗口，无法定位调节面板");
            return;
        }
        
        int panelWidth = 550;
        int rightMargin = 150;  // 与其他方法中保持一致
        
        // 计算面板在主窗口中的位置
        QPoint contentPos = m_contentWidget->mapToGlobal(QPoint(0, 0));
        QPoint mainWindowPos = mainWindow->mapFromGlobal(contentPos);
        int panelX = mainWindowPos.x() + m_contentWidget->width() - panelWidth - rightMargin;
        int panelY = mainWindowPos.y() + 80;  // 从60px增加到80px，适应更高的状态栏
        
        LOG_INFO(QString("设置调节面板位置: (%1, %2), 大小: %3x%4").arg(panelX).arg(panelY).arg(panelWidth).arg(800));
        
        // 设置面板位置
        m_adjustmentPanel->setGeometry(panelX, panelY, panelWidth, 800);
        
        // 确保控件可以接收鼠标事件
        m_adjustmentPanel->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        m_adjustmentPanel->setFocusPolicy(Qt::StrongFocus);
        m_adjustmentPanel->setMouseTracking(true);
        
        // 显示控件并确保在最上层
        m_adjustmentPanel->show();
        m_adjustmentPanel->raise();
        m_adjustmentPanel->activateWindow();
        
        // 加载当前设置
        loadCurrentSettings();
        LOG_INFO("调节面板已显示");
        
        // 显示提示
        showToast(this, "调节面板已显示", 2000);
    } else {
        m_adjustmentPanel->hide();
        LOG_INFO("调节面板已隐藏");
        
        // 显示提示
        showToast(this, "调节面板已隐藏", 2000);
    }
}

QLabel* HomePage::createLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setStyleSheet("color: white;");
    return label;
}

QSlider* HomePage::createSlider(int min, int max, int value)
{
    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(value);
    
    // 确保滑块可以接收鼠标事件
    slider->setFocusPolicy(Qt::StrongFocus);
    slider->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    slider->setMouseTracking(true);
    
    return slider;
}

void HomePage::resetToDefaults()
{
    LOG_INFO("重置相机参数到默认值");
    
    // 重置滑块值
    if (m_sliders.contains("brightness")) m_sliders["brightness"]->setValue(0);
    if (m_sliders.contains("contrast")) m_sliders["contrast"]->setValue(0);
    if (m_sliders.contains("saturation")) m_sliders["saturation"]->setValue(50);
    if (m_sliders.contains("exposure_time_absolute")) m_sliders["exposure_time_absolute"]->setValue(3);
    if (m_sliders.contains("white_balance_temperature")) m_sliders["white_balance_temperature"]->setValue(4500);
    if (m_sliders.contains("backlight_compensation")) m_sliders["backlight_compensation"]->setValue(0);
    if (m_sliders.contains("gamma")) m_sliders["gamma"]->setValue(100);
    if (m_sliders.contains("gain")) m_sliders["gain"]->setValue(0);
    
    // 重置复选框状态
    if (m_checkBoxes.contains("auto_exposure")) m_checkBoxes["auto_exposure"]->setChecked(true);
    if (m_checkBoxes.contains("white_balance_auto_preset")) m_checkBoxes["white_balance_auto_preset"]->setChecked(false);
    
    // 应用设置
    applySettings();
    
    // 显示提示
    showToast(this, "已重置为默认设置", 2000);
}

void HomePage::applySettings()
{
    LOG_INFO("应用相机参数设置");
    
    if (!m_camerasInitialized) {
        LOG_WARNING("相机未初始化，无法应用设置");
        showToast(this, "相机未初始化，无法应用设置", 2000);
        return;
    }
    
    // 收集左相机参数
    QMap<QString, QString> leftParams;
    
    // 自动曝光
    if (m_checkBoxes.contains("auto_exposure")) {
        bool autoExposure = m_checkBoxes["auto_exposure"]->isChecked();
        leftParams["auto_exposure"] = autoExposure ? "3" : "1"; // 3=自动, 1=手动
    }
    
    // 曝光时间
    if (m_sliders.contains("exposure_time_absolute")) {
        int exposureValue = m_sliders["exposure_time_absolute"]->value();
        leftParams["exposure_time_absolute"] = QString::number(exposureValue);
    }
    
    // 自动白平衡
    if (m_checkBoxes.contains("white_balance_auto_preset")) {
        bool autoWhiteBalance = m_checkBoxes["white_balance_auto_preset"]->isChecked();
        leftParams["white_balance_auto_preset"] = autoWhiteBalance ? "1" : "0"; // 1=自动, 0=手动
    }
    
    // 白平衡温度
    if (m_sliders.contains("white_balance_temperature")) {
        int whiteBalanceValue = m_sliders["white_balance_temperature"]->value();
        leftParams["white_balance_temperature"] = QString::number(whiteBalanceValue);
    }
    
    // 亮度
    if (m_sliders.contains("brightness")) {
        int brightnessValue = m_sliders["brightness"]->value();
        leftParams["brightness"] = QString::number(brightnessValue);
    }
    
    // 对比度
    if (m_sliders.contains("contrast")) {
        int contrastValue = m_sliders["contrast"]->value();
        leftParams["contrast"] = QString::number(contrastValue);
    }
    
    // 饱和度
    if (m_sliders.contains("saturation")) {
        int saturationValue = m_sliders["saturation"]->value();
        leftParams["saturation"] = QString::number(saturationValue);
    }
    
    // 背光补偿
    if (m_sliders.contains("backlight_compensation")) {
        int backlightValue = m_sliders["backlight_compensation"]->value();
        leftParams["backlight_compensation"] = QString::number(backlightValue);
    }
    
    // Gamma
    if (m_sliders.contains("gamma")) {
        int gammaValue = m_sliders["gamma"]->value();
        leftParams["gamma"] = QString::number(gammaValue);
    }
    
    // 增益
    if (m_sliders.contains("gain")) {
        int gainValue = m_sliders["gain"]->value();
        leftParams["gain"] = QString::number(gainValue);
    }
    
    // 应用参数到左相机
    applyParamsToCamera(m_leftCameraId, leftParams);
    
    // 应用相同参数到右相机
    applyParamsToCamera(m_rightCameraId, leftParams);
    
    // 显示提示
    showToast(this, "设置已应用", 2000);
}

void HomePage::applyParamsToCamera(const QString& cameraId, const QMap<QString, QString>& params)
{
    if (cameraId.isEmpty()) {
        LOG_WARNING("相机ID为空，无法应用参数");
        return;
    }
    
    LOG_INFO(QString("应用参数到相机: %1").arg(cameraId));
    
    QFileInfo deviceFile(cameraId);
    if (!deviceFile.exists()) {
        LOG_WARNING(QString("相机设备不存在: %1").arg(cameraId));
        return;
    }
    
    // 遍历参数并应用
    for (auto it = params.begin(); it != params.end(); ++it) {
        QString paramName = it.key();
        QString paramValue = it.value();
        
        // 构建v4l2-ctl命令
        QStringList args;
        args << "-d" << cameraId << "-c" << QString("%1=%2").arg(paramName).arg(paramValue);
        
        LOG_DEBUG(QString("执行命令: v4l2-ctl %1").arg(args.join(" ")));
        
        // 执行命令
        QProcess process;
        process.start("v4l2-ctl", args);
        process.waitForFinished(1000);
        
        // 检查结果
        if (process.exitCode() != 0) {
            QString errorOutput = QString::fromLocal8Bit(process.readAllStandardError());
            LOG_WARNING(QString("设置参数失败: \"%1\" = \"%2\" 错误: \"%3\"").arg(paramName).arg(paramValue).arg(errorOutput));
        } else {
            LOG_INFO(QString("成功设置参数: \"%1\" = \"%2\"").arg(paramName).arg(paramValue));
        }
    }
}

void HomePage::loadCurrentSettings()
{
    LOG_INFO("加载当前相机参数");
    
    if (!m_camerasInitialized || m_leftCameraId.isEmpty()) {
        LOG_WARNING("相机未初始化，无法加载当前设置");
        return;
    }
    
    // 使用v4l2-ctl获取当前设置
    QProcess process;
    process.start("v4l2-ctl", QStringList() << "-d" << m_leftCameraId << "-l");
    process.waitForFinished(2000);
    
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    // 解析输出并更新UI
    for (const QString &line : lines) {
        // 解析格式: name : value (min...max)
        QRegExp rx("(\\w+)\\s*:\\s*(\\d+)\\s*\\(([^)]+)\\)");
        if (rx.indexIn(line) != -1) {
            QString name = rx.cap(1);
            QString value = rx.cap(2);
            QString range = rx.cap(3);
            
            LOG_DEBUG(QString("解析参数: %1 = %2 范围: %3").arg(name).arg(value).arg(range));
            
            // 更新滑块值
            if (m_sliders.contains(name)) {
                bool ok;
                int intValue = value.toInt(&ok);
                if (ok) {
                    m_sliders[name]->setValue(intValue);
                    LOG_DEBUG(QString("更新滑块: %1 = %2").arg(name).arg(intValue));
                }
            }
            
            // 更新复选框状态
            if (name == "auto_exposure" && m_checkBoxes.contains("auto_exposure")) {
                bool autoExposure = (value == "3"); // 3=自动, 1=手动
                m_checkBoxes["auto_exposure"]->setChecked(autoExposure);
                LOG_DEBUG(QString("更新自动曝光状态: %1").arg(autoExposure));
            }
            
            if (name == "white_balance_auto_preset" && m_checkBoxes.contains("white_balance_auto_preset")) {
                bool autoWhiteBalance = (value == "1"); // 1=自动, 0=手动
                m_checkBoxes["white_balance_auto_preset"]->setChecked(autoWhiteBalance);
                LOG_DEBUG(QString("更新自动白平衡状态: %1").arg(autoWhiteBalance));
            }
        }
    }
}

// 添加更新状态栏帧率的方法
void HomePage::updateStatusBarFps(float leftFps, float rightFps)
{
    // 查找主窗口
    QWidget* mainWindow = this->window();
    if (!mainWindow) {
        return;
    }
    
    // 查找状态栏
    QList<StatusBar*> statusBars = mainWindow->findChildren<StatusBar*>();
    if (statusBars.isEmpty()) {
        return;
    }
    
    // 更新第一个找到的状态栏
    StatusBar* statusBar = statusBars.first();
    statusBar->updateFpsDisplay(leftFps, rightFps);
}

void HomePage::openFileDialog()
{
    // 如果路径选择器存在，直接调用其showFileDialog方法
    if (m_pathSelector) {
        m_pathSelector->showFileDialog();
        LOG_INFO("文件选择对话框已打开");
    } else {
        LOG_WARNING("无法找到路径选择器");
    }
}

void HomePage::captureAndSaveImages()
{
    // 防抖检查：如果正在拍照，忽略此次请求
    if (m_isCapturing) {
        LOG_INFO("拍照正在进行中，忽略重复请求");
        showToast(this, "拍照正在进行中，请稍候...", 1000);
        return;
    }

    if (!m_camerasInitialized) {
        showToast(this, "相机未初始化，无法截图", 2000);
        LOG_WARNING("相机未初始化，无法截图");
        return;
    }

    if (m_currentWorkPath.isEmpty()) {
        showToast(this, "保存路径未设置，无法截图", 2000);
        LOG_WARNING("保存路径未设置，无法截图");
        return;
    }

    // 设置拍照状态，启动防抖定时器
    m_isCapturing = true;
    m_captureDebounceTimer->start();
    LOG_INFO("开始拍照，设置防抖保护");
    
    try {
        LOG_INFO("开始截图...");
        
        // 获取相机管理器单例
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        
        // 使用同步帧获取方式
        std::map<std::string, cv::Mat> frames;
        std::map<std::string, int64_t> timestamps;
        
        // 获取同步帧，设置50ms的同步阈值
        bool success = cameraManager.getSyncFrames(frames, timestamps, 50, SyncMode::LOW_LATENCY);
        if (!success) {
            showToast(this, "获取同步帧失败", 2000);
            LOG_WARNING("获取同步帧失败");

            // 获取同步帧失败时立即重置拍照状态
            m_isCapturing = false;
            m_captureDebounceTimer->stop();
            LOG_INFO("获取同步帧失败，立即重置拍照状态");
            return;
        }

        // 验证图像
        if (frames.empty()) {
            showToast(this, "未获取到有效图像", 2000);
            LOG_WARNING("未获取到有效图像");

            // 未获取到有效图像时立即重置拍照状态
            m_isCapturing = false;
            m_captureDebounceTimer->stop();
            LOG_INFO("未获取到有效图像，立即重置拍照状态");
            return;
        }
        
        // 保存图像
        bool leftSaved = false;
        bool rightSaved = false;
        
        // 保存左相机图像
        auto leftIt = frames.find(m_leftCameraId.toStdString());
        if (leftIt != frames.end() && !leftIt->second.empty()) {
            leftSaved = saveImage(leftIt->second, "左相机");
        }
        
        // 保存右相机图像
        auto rightIt = frames.find(m_rightCameraId.toStdString());
        if (rightIt != frames.end() && !rightIt->second.empty()) {
            rightSaved = saveImage(rightIt->second, "右相机");
        }
        
        // 显示结果
        if (leftSaved && rightSaved) {
            showToast(this, "左右相机图像已保存", 2000);
            LOG_INFO("左右相机图像已保存");
        } else if (leftSaved) {
            showToast(this, "左相机图像已保存", 2000);
            LOG_INFO("左相机图像已保存");
        } else if (rightSaved) {
            showToast(this, "右相机图像已保存", 2000);
            LOG_INFO("右相机图像已保存");
        } else {
            showToast(this, "截图失败，无法保存图像", 2000);
            LOG_WARNING("截图失败，无法保存图像");
        }
    } catch (const std::exception& e) {
        showToast(this, QString("截图异常: %1").arg(e.what()), 2000);
        LOG_ERROR(QString("截图异常: %1").arg(e.what()));

        // 出现异常时立即重置拍照状态，不等待定时器
        m_isCapturing = false;
        m_captureDebounceTimer->stop();
        LOG_INFO("拍照异常，立即重置拍照状态");
    } catch (...) {
        showToast(this, "截图时发生未知异常", 2000);
        LOG_ERROR("截图时发生未知异常");

        // 出现异常时立即重置拍照状态，不等待定时器
        m_isCapturing = false;
        m_captureDebounceTimer->stop();
        LOG_INFO("拍照异常，立即重置拍照状态");
    }

    // 注意：正常情况下，拍照状态会由防抖定时器超时后自动重置
    // 这里不需要手动重置，除非出现异常
}

bool HomePage::saveImage(const cv::Mat& image, const QString& cameraName)
{
    if (image.empty() || m_currentWorkPath.isEmpty()) {
        return false;
    }
    
    try {
        // 验证图像格式
        if (image.type() != CV_8UC3 && image.type() != CV_8UC1) {
            LOG_ERROR(QString("不支持的图像格式: %1").arg(image.type()));
            return false;
        }
        
        // 确保目录存在
        QDir dir(m_currentWorkPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                LOG_ERROR(QString("无法创建目录: %1").arg(m_currentWorkPath));
                return false;
            }
        }
        
        // 生成文件名：日期时间_相机名.jpg
        QDateTime now = QDateTime::currentDateTime();
        QString timestamp = now.toString("yyyyMMdd_HHmmss");
        QString filename = QString("%1_%2.jpg").arg(timestamp).arg(cameraName);
        QString filepath = m_currentWorkPath + "/" + filename;
        
        // 检查文件是否已存在
        QFileInfo fileInfo(filepath);
        if (fileInfo.exists()) {
            // 如果文件已存在，添加随机数
            // 使用现代的QRandomGenerator替代已弃用的qsrand/qrand
            int randomNum = QRandomGenerator::global()->bounded(1000);
            filename = QString("%1_%2_%3.jpg").arg(timestamp).arg(cameraName).arg(randomNum);
            filepath = m_currentWorkPath + "/" + filename;
        }
        
        // 保存图像
        bool success = cv::imwrite(filepath.toStdString(), image);
        if (success) {
            LOG_INFO(QString("图像已保存: %1").arg(filepath));
            return true;
        } else {
            LOG_ERROR(QString("保存图像失败: %1").arg(filepath));
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("保存图像异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("保存图像时发生未知异常");
        return false;
    }
}

QString HomePage::getLeftCameraId() const
{
    return m_leftCameraId;
}

QString HomePage::getRightCameraId() const
{
    return m_rightCameraId;
}

void HomePage::onFrameReceived(const std::string& cameraId, const cv::Mat& frame, int64_t timestamp)
{
    try {
        // 只处理当前可见页面的帧
        if (!m_camerasInitialized || !isVisible()) {
            return;
        }
        
        // 检查帧是否有效
        if (frame.empty()) {
            return;
        }
        
        // 创建要显示的图像（默认使用原始帧）
        cv::Mat displayImage = frame.clone();
        
        // 如果是左相机的帧且检测功能已启用，则处理检测逻辑
        if (cameraId == m_leftCameraId.toStdString() && m_detectionEnabled) {
            // 每20帧记录一次日志，记录实时检测状态
            static int statusLogCounter = 0;
            if (++statusLogCounter % 20 == 0) {
                LOG_DEBUG(QString("实时检测状态：已启用=%1, 正在处理=%2, 左相机ID=%3")
                          .arg(m_detectionEnabled ? "是" : "否")
                          .arg(m_processingDetection.load() ? "是" : "否")
                          .arg(QString::fromStdString(cameraId)));
            }
            
            // 实时检测逻辑
            // 如果当前没有正在处理的检测请求，则提交新的检测请求
            if (!m_processingDetection.load()) {
                LOG_DEBUG("提交新的实时检测请求");
                // 提交当前帧进行检测
                submitFrameForDetection(frame, cameraId);
            }
        }
        
        // 将OpenCV图像转换为QImage
        QImage qimg = matToQImage(displayImage);
        
        // 如果转换失败，跳过此帧
        if (qimg.isNull()) {
            return;
        }
        
        // 根据相机ID更新对应的标签
        if (isVisible()) {
            if (cameraId == m_leftCameraId.toStdString() && m_leftCameraView && m_leftCameraView->isVisible()) {
                QSize labelSize = m_leftCameraView->size();
                if (labelSize.width() > 10 && labelSize.height() > 10) {
                    m_leftCameraView->setPixmap(QPixmap::fromImage(qimg).scaled(
                        labelSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                }
            } else if (cameraId == m_rightCameraId.toStdString() && m_rightCameraView && m_rightCameraView->isVisible()) {
                QSize labelSize = m_rightCameraView->size();
                if (labelSize.width() > 10 && labelSize.height() > 10) {
                    m_rightCameraView->setPixmap(QPixmap::fromImage(qimg).scaled(
                        labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
        }
        
        // 每秒更新一次帧率显示
        static auto lastFpsUpdateTime = std::chrono::steady_clock::now();
        auto currentTime = std::chrono::steady_clock::now();
        auto fpsElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFpsUpdateTime).count();
        
        if (fpsElapsed >= 1000) {
            // 获取相机管理器实例
            using namespace SmartScope::Core::CameraUtils;
            MultiCameraManager& cameraManager = MultiCameraManager::instance();
            
            // 获取左右相机的帧率
            float leftFps = 0.0f;
            float rightFps = 0.0f;
            
            // 获取相机信息
            if (!m_leftCameraId.isEmpty()) {
                auto leftCameraInfo = cameraManager.getCameraInfo(m_leftCameraId.toStdString());
                leftFps = leftCameraInfo.fps;
            }
            
            if (!m_rightCameraId.isEmpty()) {
                auto rightCameraInfo = cameraManager.getCameraInfo(m_rightCameraId.toStdString());
                rightFps = rightCameraInfo.fps;
            }
            
            // 更新状态栏中的帧率显示
            updateStatusBarFps(leftFps, rightFps);
            
            lastFpsUpdateTime = currentTime;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("主页更新相机视图时发生异常: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("主页更新相机视图时发生未知异常");
    }
}

// 实现submitFrameForDetection方法
void HomePage::submitFrameForDetection(const cv::Mat& frame, const std::string& cameraId)
{
    // 如果检测功能没有启用，直接返回
    if (!m_detectionEnabled) {
        return;
    }
    
    if (!YOLOv8Service::instance().isInitialized()) {
        LOG_WARNING("跳过检测请求：YOLOv8服务未初始化");
        return;
    }
    
    // 如果正在处理检测请求，跳过此帧
    if (m_processingDetection.load()) {
        return;
    }
    
    // 设置标志，表示正在处理检测请求
    m_processingDetection.store(true);
    
    try {
        // 检查输入图像是否有效
        if (frame.empty()) {
            LOG_WARNING("跳过检测请求：输入图像为空");
            m_processingDetection.store(false);
            return;
        }
        
        // 确保图像格式正确
        cv::Mat processImage;
        if (frame.channels() == 4) {
            // 如果是4通道图像，转换为3通道
            cv::cvtColor(frame, processImage, cv::COLOR_BGRA2BGR);
        } else if (frame.channels() == 1) {
            // 如果是单通道图像，转换为3通道
            cv::cvtColor(frame, processImage, cv::COLOR_GRAY2BGR);
        } else {
            // 直接使用原图像
            processImage = frame.clone();
        }
        
        // 调整图像大小以适应模型输入 (640x640是YOLOv8的标准输入尺寸)
        cv::Mat resizedImage;
        cv::resize(processImage, resizedImage, cv::Size(640, 640));
        
        // 创建检测请求
        YOLOv8Request request;
        request.image = resizedImage; // 使用调整大小后的图像
        request.save_path = ""; // 不保存结果
        request.session_id = YOLOv8Service::instance().resetSessionId(); // 获取新的会话ID
        request.request_id = YOLOv8Service::instance().getNextRequestId(); // 获取新的请求ID
        request.confidence_threshold = 0.05f; // 使用更低的置信度阈值，增加检测成功率
        
        // 保存当前会话ID
        m_lastDetectionSessionId = request.session_id;
        
        // 提交检测请求
        YOLOv8Service::instance().submitRequest(request);
        
        // 每10帧记录一次日志，大幅增加日志频率
        static int logCounter = 0;
        if (++logCounter % 10 == 0) {
            LOG_INFO(QString("已提交实时帧进行检测，会话ID：%1，原始帧尺寸：%2x%3，调整后尺寸：640x640")
                    .arg(request.session_id)
                    .arg(frame.cols).arg(frame.rows));
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("提交检测请求时发生异常：%1").arg(e.what()));
        m_processingDetection.store(false); // 重置标志
    }
    catch (...) {
        LOG_ERROR("提交检测请求时发生未知异常");
        m_processingDetection.store(false); // 重置标志
    }
}

// 实现onDetectionCompleted方法
void HomePage::onDetectionCompleted(const SmartScope::YOLOv8Result& result)
{
    // 检查会话ID是否匹配
    if (result.session_id != m_lastDetectionSessionId) {
        LOG_DEBUG(QString("忽略过时的检测结果，当前会话ID: %1，结果会话ID: %2")
                 .arg(m_lastDetectionSessionId).arg(result.session_id));
        m_processingDetection = false;
        
        // 即使是过时的结果，也立即开始下一次检测
        if (m_detectionEnabled) {
            startNextDetection();
        }
        return;
    }

    // 重置处理标志
    m_processingDetection = false;
    
    // 检查检测是否成功
    if (!result.success) {
        LOG_ERROR("目标检测失败");
        
        // 即使检测失败，也立即开始下一次检测
        if (m_detectionEnabled) {
            startNextDetection();
        }
        return;
    }
    
    // 记录检测结果
    QString detectionMode = m_detectionEnabled ? "实时检测" : "测试检测";
    LOG_INFO(QString("%1完成 - 会话ID: %2, 检测到 %3 个目标")
             .arg(detectionMode)
             .arg(result.session_id)
             .arg(result.detections.size()));

    // 检查结果图像是否有效
    cv::Mat resultImage;
    if (result.result_image.empty()) {
        LOG_WARNING(QString("[%1] 检测结果图像为空，使用原始帧绘制检测框").arg(detectionMode));
        
        // 如果服务没有提供结果图像，我们需要获取原始帧来绘制检测框
        // 获取相机管理器实例
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        std::map<std::string, cv::Mat> frames;
        std::map<std::string, int64_t> timestamps;
        
        if (cameraManager.getSyncFrames(frames, timestamps, false)) {
            auto it = frames.find(m_leftCameraId.toStdString());
            if (it != frames.end() && !it->second.empty()) {
                resultImage = it->second.clone();
                LOG_DEBUG("使用当前相机帧作为检测结果基础图像");
            } else {
                LOG_WARNING("无法获取当前相机帧，跳过检测结果显示");
                if (m_detectionEnabled) {
                    startNextDetection();
                }
                return;
            }
        } else {
            LOG_WARNING("无法获取同步帧，跳过检测结果显示");
            if (m_detectionEnabled) {
                startNextDetection();
            }
            return;
        }
    } else {
        // 使用服务提供的结果图像作为基础，但需要调整尺寸
        // 检测结果图像是640x640，需要调整为原始帧尺寸
        MultiCameraManager& cameraManager = MultiCameraManager::instance();
        std::map<std::string, cv::Mat> frames;
        std::map<std::string, int64_t> timestamps;
        
        cv::Size targetSize(1280, 720); // 默认目标尺寸
        
        // 尝试获取当前帧以确定正确的尺寸
        if (cameraManager.getSyncFrames(frames, timestamps, false)) {
            auto it = frames.find(m_leftCameraId.toStdString());
            if (it != frames.end() && !it->second.empty()) {
                targetSize = cv::Size(it->second.cols, it->second.rows);
                LOG_DEBUG(QString("使用当前帧尺寸作为目标尺寸: %1x%2").arg(targetSize.width).arg(targetSize.height));
            }
        }
        
        // 将640x640的检测结果调整为原始帧尺寸
        cv::resize(result.result_image, resultImage, targetSize);
        LOG_DEBUG(QString("已将检测结果从640x640调整为%1x%2").arg(targetSize.width).arg(targetSize.height));
    }
    
    // 重新绘制检测框以保证醒目（在正确尺寸的图像上）
    if (!result.detections.empty()) {
        // 记录自定义绘制开始
        LOG_DEBUG(QString("[%1] 开始在%2x%3图像上绘制自定义检测框，检测到 %4 个目标")
                 .arg(detectionMode)
                 .arg(resultImage.cols)
                 .arg(resultImage.rows)
                 .arg(result.detections.size()));
        
        // 获取图像尺寸
        int imgHeight = resultImage.rows;
        int imgWidth = resultImage.cols;
        
        // 原始检测是在640x640上进行的，需要计算缩放比例到当前图像尺寸
        float scaleX = static_cast<float>(imgWidth) / 640.0f;
        float scaleY = static_cast<float>(imgHeight) / 640.0f;
        
        LOG_DEBUG(QString("缩放比例: scaleX=%1, scaleY=%2").arg(scaleX).arg(scaleY));
        
        // 为每个检测目标绘制边界框和标签
        for (const auto& detection : result.detections) {
            // 只绘制置信度高于阈值的目标
            if (detection.confidence < m_detectionConfidenceThreshold) {
                continue;
            }
            
            // 计算缩放后的边界框坐标
            int x = static_cast<int>(detection.box.x * scaleX);
            int y = static_cast<int>(detection.box.y * scaleY);
            int width = static_cast<int>(detection.box.width * scaleX);
            int height = static_cast<int>(detection.box.height * scaleY);
            
            // 确保坐标在图像范围内
            x = std::max(0, std::min(x, imgWidth - 1));
            y = std::max(0, std::min(y, imgHeight - 1));
            width = std::min(width, imgWidth - x);
            height = std::min(height, imgHeight - y);
            
            // 根据类别选择颜色
            cv::Scalar color;
            // 使用固定的几种醒目颜色
            static const std::vector<cv::Scalar> colors = {
                cv::Scalar(0, 255, 0),    // 绿色
                cv::Scalar(255, 0, 0),    // 蓝色
                cv::Scalar(0, 0, 255),    // 红色
                cv::Scalar(255, 255, 0),  // 青色
                cv::Scalar(0, 255, 255),  // 黄色
                cv::Scalar(255, 0, 255)   // 品红色
            };
            
            // 根据类别名称计算哈希值，用于选择颜色
            size_t hash = std::hash<std::string>{}(detection.class_name);
            color = colors[hash % colors.size()];
            
            // 绘制边界框，线条粗度为4，更醒目
            cv::rectangle(resultImage, cv::Rect(x, y, width, height), color, 4);
            
            // 准备标签文本：类别名称 + 置信度
            std::string labelText = detection.class_name + " " + 
                                   std::to_string(static_cast<int>(detection.confidence * 100)) + "%";
            
            // 获取文本大小，用于绘制背景矩形
            int baseLine = 0;
            double fontScale = 0.8; // 增大字体
            int thickness = 2;
            cv::Size labelSize = cv::getTextSize(labelText, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseLine);
            
            // 绘制标签背景
            cv::rectangle(resultImage, cv::Point(x, y - labelSize.height - 15), 
                          cv::Point(x + labelSize.width + 15, y), 
                          color, -1);  // -1表示填充矩形
            
            // 绘制标签文本
            cv::putText(resultImage, labelText, cv::Point(x + 8, y - 8), 
                       cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255), thickness);
            
            LOG_DEBUG(QString("绘制检测框: %1 [%2,%3,%4,%5] 置信度=%6")
                     .arg(QString::fromStdString(detection.class_name))
                     .arg(x).arg(y).arg(width).arg(height)
                     .arg(detection.confidence, 0, 'f', 3));
        }
        
        LOG_DEBUG(QString("[%1] 完成自定义检测框绘制").arg(detectionMode));
    }
    
    // 保存检测结果数据（非阻塞方式）
    if (m_detectionMutex.try_lock()) {
        // 只保存检测结果数据，不保存图像
        m_lastDetectionResults = result.detections;
        
        // 减少日志频率，避免影响性能
        static int updateCount = 0;
        if (++updateCount % 50 == 0) {
            LOG_INFO(QString("[%1] 已保存检测结果数据，检测目标数: %2")
                    .arg(detectionMode)
                    .arg(result.detections.size()));
        }
        
        m_detectionMutex.unlock();
    } else {
        // 如果无法获取锁，跳过本次结果保存，避免阻塞
        static int skipCount = 0;
        if (++skipCount % 20 == 0) {
            LOG_DEBUG("检测结果保存被跳过，避免阻塞显示");
        }
    }
    
    // 如果是实时检测模式，立即开始下一次检测
    if (m_detectionEnabled) {
        startNextDetection();
    }
}

// 添加新方法：开始下一次检测
void HomePage::startNextDetection()
{
    // 增加延迟到100ms，给相机足够时间准备新帧
    QTimer::singleShot(100, this, [this]() {
        try {
            if (!m_detectionEnabled) {
                LOG_DEBUG("检测已禁用，不再提交新的检测请求");
                return;
            }
            
            LOG_DEBUG("正在获取最新帧进行下一次检测...");
            
            // 获取相机管理器实例
            MultiCameraManager& cameraManager = MultiCameraManager::instance();
            
            // 使用更长的超时时间，增加获取成功率
            std::map<std::string, cv::Mat> frames;
            std::map<std::string, int64_t> timestamps;
            
            // 增加重试次数和延迟，使用特殊的获取策略
            int maxRetries = 5;  // 从3次增加到5次
            int retryCount = 0;
            bool frameObtained = false;
            
            while (!frameObtained && retryCount < maxRetries) {
                if (retryCount > 0) {
                    LOG_INFO(QString("尝试第%1次重新获取同步帧...").arg(retryCount));
                    // 增加重试间隔，指数级增长
                    QThread::msleep(100 + retryCount * 50);  // 150ms, 200ms, 250ms, 300ms...
                }
                
                // 添加更长的超时时间，最多等待100ms，并使用低延迟模式获取最新帧
                frameObtained = cameraManager.getSyncFrames(frames, timestamps, 100, SyncMode::LOW_LATENCY);
                
                if (!frameObtained) {
                    LOG_WARNING(QString("第%1次获取同步帧失败").arg(retryCount + 1));
                    retryCount++;
                }
            }
            
            if (!frameObtained) {
                LOG_WARNING("多次尝试获取同步帧均失败，无法提交下一次检测请求");
                m_processingDetection.store(false);
                
                // 添加相机重置逻辑
                if (retryCount >= maxRetries) {
                    LOG_INFO("尝试重置相机以恢复连接...");
                    // 短暂禁用相机
                    disableCameras();
                    
                    // 延迟1000ms后重新启用相机 - 给相机更多恢复时间
                    QTimer::singleShot(1000, this, [this]() {
                        enableCameras();
                        LOG_INFO("相机已重置，等待新帧...");
                        
                        // 等待相机稳定后再尝试新的检测
                        QTimer::singleShot(500, this, [this]() {
                            // 重置处理标志，允许下一次处理
                            m_processingDetection.store(false);
                            
                            // 如果检测仍然启用，尝试再次开始新的检测循环
                            if (m_detectionEnabled) {
                                LOG_INFO("重置后尝试重新开始检测循环");
                                
                                // 使用较长延迟，给相机足够的恢复时间
                                QTimer::singleShot(200, this, [this]() {
                                    // 获取相机管理器实例
                                    MultiCameraManager& cameraManager = MultiCameraManager::instance();
                                    
                                    // 尝试获取帧，使用更长的超时
                                    std::map<std::string, cv::Mat> frames;
                                    std::map<std::string, int64_t> timestamps;
                                    
                                    if (cameraManager.getSyncFrames(frames, timestamps, 200, SyncMode::LOW_LATENCY)) {
                                        // 查找左相机的帧
                                        auto it = frames.find(m_leftCameraId.toStdString());
                                        if (it != frames.end() && !it->second.empty()) {
                                            // 提交帧进行检测
                                            submitFrameForDetection(it->second, m_leftCameraId.toStdString());
                                            LOG_INFO("相机重置后成功提交新的检测请求");
                                        }
                                    }
                                });
                            }
                        });
                    });
                }
                return;
            }
            
            // 查找左相机的帧
            auto it = frames.find(m_leftCameraId.toStdString());
            if (it != frames.end() && !it->second.empty()) {
                // 提交下一帧进行检测
                submitFrameForDetection(it->second, m_leftCameraId.toStdString());
                
                // 每20次记录一次日志
                static int nextDetectionCounter = 0;
                if (++nextDetectionCounter % 20 == 0) {
                    LOG_INFO("成功提交下一帧进行检测");
                }
            } else {
                LOG_WARNING("无法获取左相机帧进行下一次检测");
                
                // 重置处理标志，以便下次帧回调可以尝试提交
                m_processingDetection.store(false);
            }
        } catch (const std::exception& e) {
            LOG_ERROR(QString("提交下一次检测请求时发生异常：%1").arg(e.what()));
            m_processingDetection.store(false);
        } catch (...) {
            LOG_ERROR("提交下一次检测请求时发生未知异常");
            m_processingDetection.store(false);
        }
    });
}

// 实现toggleObjectDetection方法
void HomePage::toggleObjectDetection(bool enabled)
{
    LOG_INFO(QString("切换目标检测状态：%1 -> %2")
            .arg(m_detectionEnabled ? "启用" : "禁用")
            .arg(enabled ? "启用" : "禁用"));
            
    if (m_detectionEnabled == enabled) {
        LOG_INFO("检测状态未变，不做处理");
        return; // 状态未变，不做处理
    }
    
    // 更新状态标志
    m_detectionEnabled = enabled;
    
    if (enabled) {
        // 启用检测前先检查相机是否初始化
        if (!m_camerasInitialized) {
            LOG_ERROR("相机未初始化，无法启用目标检测");
            showToast(this, "相机未初始化，无法启用目标检测", 2000);
            m_detectionEnabled = false;
            updateDetectionButton(false); // 更新按钮状态
            return;
        }
        
        // 检查左相机是否可用
        if (m_leftCameraId.isEmpty()) {
            LOG_ERROR("左相机未找到，无法启用目标检测");
            showToast(this, "左相机未找到，无法启用目标检测", 2000);
            m_detectionEnabled = false;
            updateDetectionButton(false); // 更新按钮状态
            return;
        }
        
        // 启用检测前先检查YOLOv8服务状态
        if (!YOLOv8Service::instance().isInitialized()) {
            LOG_INFO("YOLOv8服务未初始化，尝试初始化...");
            
            // 尝试初始化YOLOv8服务 - 修正模型和标签文件名
            QString modelPath = QCoreApplication::applicationDirPath() + "/models/yolov8m.rknn";
            QString labelPath = QCoreApplication::applicationDirPath() + "/models/coco_80_labels_list.txt";
            
            LOG_INFO(QString("使用模型: %1").arg(modelPath));
            LOG_INFO(QString("使用标签: %1").arg(labelPath));
            
            if (!YOLOv8Service::instance().initialize(modelPath, labelPath)) {
                LOG_ERROR("YOLOv8服务初始化失败，无法启用目标检测");
                showToast(this, "YOLOv8服务初始化失败，无法启用目标检测", 2000);
                m_detectionEnabled = false;
                updateDetectionButton(false); // 更新按钮状态
                return;
            }
            
            LOG_INFO("YOLOv8服务初始化成功");
        }
        
        if (!YOLOv8Service::instance().isRunning()) {
            LOG_WARNING("YOLOv8服务未运行，无法启用目标检测");
            showToast(this, "YOLOv8服务未运行，无法启用目标检测", 2000);
            m_detectionEnabled = false;
            updateDetectionButton(false); // 更新按钮状态
            return;
        }
        
        // 强制重置处理标志，确保可以开始新的检测
        m_processingDetection.store(false);
        LOG_INFO("已重置处理标志，可以开始新的检测");
        
        // 清除旧的检测结果
        {
            QMutexLocker locker(&m_detectionMutex);
            if (!m_lastDetectionResults.empty()) {
                m_lastDetectionResults.clear();
                LOG_INFO("已清除旧的检测结果");
            }
        }
        
        // 确保页面可见
        // 只有当页面可见时才开始检测循环
        if (isVisible()) {
            LOG_INFO("页面可见，开始初始检测请求");
            startNextDetection();
        } else {
            LOG_INFO("页面不可见，等待显示后再开始检测");
        }
        
        // 显示提示信息
        showToast(this, "已启用目标检测", 1500);
        LOG_INFO("已启用目标检测功能");
                    } else {
        // 停止检测 - 仅需设置标志，检测循环会自动停止
        LOG_INFO("已停用目标检测功能");
        
        // 显示提示信息
        showToast(this, "已停用目标检测", 1500);
    }
    
    // 更新按钮状态
    updateDetectionButton(m_detectionEnabled);
    
    // 发送目标检测状态变更信号
    emit objectDetectionEnabledChanged(m_detectionEnabled);
}

/**
 * @brief 更新目标检测按钮状态
 * @param checked 是否选中
 */
void HomePage::updateDetectionButton(bool checked)
{
    // 获取主窗口
    MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
    if (!mainWindow) {
        LOG_WARNING("无法获取主窗口，无法更新检测按钮状态");
        return;
    }
    
    // 获取工具栏
    ToolBar* toolBar = mainWindow->getToolBar();
    if (!toolBar) {
        LOG_WARNING("无法获取工具栏，无法更新检测按钮状态");
        return;
    }
    
    // 获取检测按钮
    QPushButton* detectionButton = toolBar->getButton("detectionButton");
    if (detectionButton) {
        // 断开当前信号连接，防止循环触发
        detectionButton->blockSignals(true);
        
        // 设置按钮状态
        detectionButton->setChecked(checked);
        
        // 恢复信号连接
        detectionButton->blockSignals(false);
        
        LOG_INFO(QString("已更新检测按钮状态: %1").arg(checked ? "启用" : "禁用"));
    } else {
        LOG_WARNING("无法获取检测按钮，无法更新状态");
    }
}

// 实现isObjectDetectionEnabled方法
bool HomePage::isObjectDetectionEnabled() const
{
    return m_detectionEnabled;
}

// 实现在指定图像上绘制检测结果的方法
void HomePage::drawDetectionResults(cv::Mat& image, const std::vector<SmartScope::YOLOv8Result::Detection>& detections)
{
    if (image.empty() || detections.empty()) {
        return;
    }

    // 获取图像尺寸
    int imgHeight = image.rows;
    int imgWidth = image.cols;

    // 原始检测是在640x640上进行的，需要计算缩放比例到当前图像尺寸
    float scaleX = static_cast<float>(imgWidth) / 640.0f;
    float scaleY = static_cast<float>(imgHeight) / 640.0f;
    
    // 为每个检测目标绘制边界框和标签
    for (const auto& detection : detections) {
        // 只绘制置信度高于阈值的目标
        if (detection.confidence < m_detectionConfidenceThreshold) {
            continue;
        }
        
        // 计算缩放后的边界框坐标
        int x = static_cast<int>(detection.box.x * scaleX);
        int y = static_cast<int>(detection.box.y * scaleY);
        int width = static_cast<int>(detection.box.width * scaleX);
        int height = static_cast<int>(detection.box.height * scaleY);
        
        // 确保坐标在图像范围内
        x = std::max(0, std::min(x, imgWidth - 1));
        y = std::max(0, std::min(y, imgHeight - 1));
        width = std::min(width, imgWidth - x);
        height = std::min(height, imgHeight - y);
        
        // 根据类别选择颜色
        static const std::vector<cv::Scalar> colors = {
            cv::Scalar(0, 255, 0),    // 绿色
            cv::Scalar(255, 0, 0),    // 蓝色
            cv::Scalar(0, 0, 255),    // 红色
            cv::Scalar(255, 255, 0),  // 青色
            cv::Scalar(0, 255, 255),  // 黄色
            cv::Scalar(255, 0, 255)   // 品红色
        };
        
        // 根据类别名称计算哈希值，用于选择颜色
        size_t hash = std::hash<std::string>{}(detection.class_name);
        cv::Scalar color = colors[hash % colors.size()];
        
        // 绘制边界框，线条粗度为4，更醒目
        cv::rectangle(image, cv::Rect(x, y, width, height), color, 4);
        
        // 准备标签文本：类别名称 + 置信度
        std::string labelText = detection.class_name + " " + 
                               std::to_string(static_cast<int>(detection.confidence * 100)) + "%";
        
        // 获取文本大小，用于绘制背景矩形
        int baseLine = 0;
        double fontScale = 0.8;
        int thickness = 2;
        cv::Size labelSize = cv::getTextSize(labelText, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseLine);

        // 计算标签位置，确保标签始终在屏幕内
        int labelPadding = 8;
        int labelHeight = labelSize.height + labelPadding * 2;
        int labelWidth = labelSize.width + labelPadding * 2;

        // 默认标签位置（边界框上方）
        int labelX = x;
        int labelY = y - labelHeight;
        int textX = x + labelPadding;
        int textY = y - labelPadding;

        // 检查标签是否会超出屏幕边界，并调整位置

        // 1. 检查顶部边界：如果标签会超出顶部，则放在边界框下方
        if (labelY < 0) {
            labelY = y + height;
            textY = y + height + labelSize.height + labelPadding;
        }

        // 2. 检查底部边界：如果标签会超出底部，则放在边界框内部顶部
        if (labelY + labelHeight > imgHeight) {
            labelY = y;
            textY = y + labelSize.height + labelPadding;
        }

        // 3. 检查左边界：如果标签会超出左边，则调整到屏幕内
        if (labelX < 0) {
            labelX = 0;
            textX = labelPadding;
        }

        // 4. 检查右边界：如果标签会超出右边，则向左调整
        if (labelX + labelWidth > imgWidth) {
            labelX = imgWidth - labelWidth;
            textX = labelX + labelPadding;
        }

        // 最终边界检查，确保标签完全在屏幕内
        labelX = std::max(0, std::min(labelX, imgWidth - labelWidth));
        labelY = std::max(0, std::min(labelY, imgHeight - labelHeight));
        textX = labelX + labelPadding;
        textY = labelY + labelSize.height + labelPadding;

        // 绘制标签背景
        cv::rectangle(image, cv::Point(labelX, labelY),
                      cv::Point(labelX + labelWidth, labelY + labelHeight),
                      color, -1);  // -1表示填充矩形

        // 绘制标签文本
        cv::putText(image, labelText, cv::Point(textX, textY),
                   cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255), thickness);
    }
}

/**
 * @brief 调整画中画位置和大小
 * @param position 画中画位置
 * @param size 画中画大小
 */
void HomePage::adjustPipView(const QPoint& position, const QSize& size)
{
    if (m_rightCameraView) {
        // 确保画中画不会超出主窗口边界
        QPoint adjustedPos = position;
        QSize adjustedSize = size;
        
        // 限制最小尺寸
        adjustedSize.setWidth(qMax(120, adjustedSize.width()));
        adjustedSize.setHeight(qMax(90, adjustedSize.height()));
        
        // 限制最大尺寸
        adjustedSize.setWidth(qMin(this->width() / 2, adjustedSize.width()));
        adjustedSize.setHeight(qMin(this->height() / 2, adjustedSize.height()));
        
        // 确保不超出窗口边界
        if (adjustedPos.x() + adjustedSize.width() > this->width()) {
            adjustedPos.setX(this->width() - adjustedSize.width());
        }
        if (adjustedPos.y() + adjustedSize.height() > this->height()) {
            adjustedPos.setY(this->height() - adjustedSize.height());
        }
        
        // 应用新的位置和尺寸
        m_rightCameraView->move(adjustedPos);
        m_rightCameraView->setFixedSize(adjustedSize);
        
        LOG_INFO(QString("调整画中画位置: (%1, %2), 尺寸: %3x%4")
                .arg(adjustedPos.x()).arg(adjustedPos.y())
                .arg(adjustedSize.width()).arg(adjustedSize.height()));
    }
}

void HomePage::toggleDistortionCorrection()
{
    m_distortionCorrectionEnabled = !m_distortionCorrectionEnabled;
    
    if (m_distortionCorrectionEnabled) {
        LOG_INFO("启用畸变校正");
        showToast(this, "畸变校正已启用", 2000);
    } else {
        LOG_INFO("禁用畸变校正");
        showToast(this, "畸变校正已禁用", 2000);
    }
}

cv::Mat HomePage::applyImageFilters(const cv::Mat& image, const std::string& cameraId)
{
    if (image.empty()) {
        return image;
    }
    
    cv::Mat result = image.clone();
    
    // 应用畸变校正滤镜
    if (m_distortionCorrectionEnabled && m_correctionManager) {
        try {
            // 只对左相机应用畸变校正
            if (cameraId == m_leftCameraId.toStdString()) {
                // 步骤1：先顺时针旋转90度（因为标定文件是旋转90度后标定的）
                cv::Mat rotatedImage;
                cv::rotate(result, rotatedImage, cv::ROTATE_90_CLOCKWISE);
                
                // 步骤2：对旋转后的图像进行畸变校正
                SmartScope::Core::CorrectionResult correctionResult = m_correctionManager->correctImages(
                    rotatedImage, cv::Mat(), SmartScope::Core::CorrectionType::DISTORTION);
                
                if (correctionResult.success && !correctionResult.correctedLeftImage.empty()) {
                    // 步骤3：校正完成后，逆时针旋转90度转回来
                    cv::rotate(correctionResult.correctedLeftImage, result, cv::ROTATE_90_COUNTERCLOCKWISE);
                } else {
                    // 如果校正失败，也要转回来
                    cv::rotate(rotatedImage, result, cv::ROTATE_90_COUNTERCLOCKWISE);
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING(QString("畸变校正失败: %1").arg(e.what()));
        }
    }
    
    // 这里可以添加其他滤镜处理
    // 例如：亮度调整、对比度调整、噪声去除等
    
    return result;
}
    
    