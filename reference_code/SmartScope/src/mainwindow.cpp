#include "mainwindow.h"
#include "statusbar.h"
#include "app/ui/page_manager.h"
#include "app/ui/navigation_bar.h"
#include "app/ui/home_page.h"
#include "inference/inference_service.hpp"
#include "infrastructure/config/config_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QApplication>
#include <QScreen>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QRect>
#include <QStackedWidget>
#include <QStackedLayout>
#include <QResizeEvent>
#include "infrastructure/logging/logger.h"
#include <QDir>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QCursor>
#include <QEvent>
#include <QMessageBox>
#include <QTimer>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QKeyEvent>
#include "app/ui/utils/dialog_utils.h"
#include "app/utils/led_controller.h"
#include "app/ui/toast_notification.h"
#include <QPushButton>
#include <QIcon>
#include <QPixmap>
#include <QPainter>

// 使用正确的命名空间
using namespace SmartScope;
using namespace SmartScope::Infrastructure;

// 定义 LOG_INFO 宏，因为 Infrastructure 命名空间不可用
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_pageManager(nullptr)
    , m_navigationBar(nullptr)
    , m_toolBar(nullptr)
    , m_statusBar(nullptr)
{
    // 设置应用程序样式
    setupUi();
    
    // 获取配置管理器实例
    auto& configManager = SmartScope::Infrastructure::ConfigManager::instance();
    
    // 获取模型路径
            QString modelPath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/models/depth_anything_v2_vits.rknn");
    LOG_INFO(QString("使用模型路径: %1").arg(modelPath));
    
    // 初始化推理服务
    if (!SmartScope::InferenceService::instance().initialize(modelPath)) {
        LOG_WARNING("推理服务初始化失败");
    } else {
        LOG_INFO("推理服务初始化成功");
    }
    
    // 初始化LED亮度
    QTimer::singleShot(500, this, [this]() {
        if (LedController::instance().isConnected()) {
            int currentPercent = LedController::instance().getCurrentBrightnessPercentage();
            // 如果当前不是最大亮度，则设置为最大亮度
            if (currentPercent < 100) {
                LOG_INFO("程序启动时设置LED亮度为100%");
                if (LedController::instance().setLightLevel(0)) { // 0是最大亮度的索引
                    showToast(this, "灯光亮度：100%", 1500);
                }
            }
        }
    });
    
    // 安装事件过滤器
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
}

SmartScope::ToolBar* MainWindow::getToolBar() const
{
    return m_toolBar;
}

void MainWindow::setupUi()
{
    // 设置窗口标题
    setWindowTitle("智能内窥镜");
    
    // 获取屏幕尺寸
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    
    // 根据屏幕分辨率设置窗口大小
    resize(screenGeometry.width(), screenGeometry.height());
    
    // 设置窗口为全屏显示
    setWindowState(Qt::WindowFullScreen);
    
    LOG_INFO(QString("屏幕分辨率: %1x%2").arg(screenGeometry.width()).arg(screenGeometry.height()));
    LOG_INFO(QString("窗口大小: %1x%2").arg(width()).arg(height()));
    
    // 设置应用字体
    QFont appFont("WenQuanYi Zen Hei", 10);
    QApplication::setFont(appFont);
    LOG_INFO(QString("当前应用字体: \"%1\"").arg(QApplication::font().family()));
    
    // 创建中央部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // 创建主布局 - 使用单层布局而不是QStackedLayout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 创建内容区域
    QWidget *contentContainer = new QWidget(centralWidget);
    contentContainer->setObjectName("contentContainer");
    contentContainer->setStyleSheet("background-color: #1E1E1E;"); // 设置内容区域背景色
    QVBoxLayout *containerLayout = new QVBoxLayout(contentContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    
    // 创建页面管理器
    m_pageManager = new PageManager(this);
    
    // 创建导航栏
    m_navigationBar = new NavigationBar(this);
    m_navigationBar->setPageManager(m_pageManager);
    
    // 设置导航栏属性
    m_navigationBar->setAutoFillBackground(false);
    m_navigationBar->setAttribute(Qt::WA_TranslucentBackground, true);
    // 设置窗口标志，确保导航栏显示在最上层
    m_navigationBar->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // 创建状态栏
    m_statusBar = new StatusBar(this);
    
    // 设置状态栏属性
    m_statusBar->setAutoFillBackground(false);
    m_statusBar->setAttribute(Qt::WA_TranslucentBackground, true);
    // 设置窗口标志，确保状态栏显示在最上层
    m_statusBar->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // 添加页面管理器到容器
    containerLayout->addWidget(m_pageManager, 1);
    
    // 创建工具栏
    m_toolBar = new ToolBar(this);
    
    // 确保工具栏可见
    m_toolBar->show();
    m_toolBar->raise();
    m_toolBar->updatePosition();
 
     // 创建录屏控制器（隐藏UI，只提供逻辑）
     m_screenRecorder = new ScreenRecorderOverlay(this);
     m_screenRecorder->hide();
 
     // 添加录屏按钮到工具栏底部，使用资源图标
     QPushButton* recordBtn = m_toolBar->addBottomButton("recordButton", ":/icons/record_start.svg", "屏幕录制");
     if (recordBtn) {
         // 本地方法：将图标转为白色
         auto makeWhiteIcon = [](const QString& path) -> QIcon {
             QPixmap pix(path);
             if (pix.isNull()) return QIcon(path);
             QPixmap white(pix.size());
             white.fill(Qt::transparent);
             QPainter p(&white);
             p.setRenderHint(QPainter::Antialiasing);
             p.setCompositionMode(QPainter::CompositionMode_SourceOver);
             p.drawPixmap(0, 0, pix);
             p.setCompositionMode(QPainter::CompositionMode_SourceIn);
             p.fillRect(white.rect(), QColor(255,255,255,255));
             p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
             p.fillRect(white.rect(), QColor(255,255,255,255));
             p.end();
             return QIcon(white);
         };
         // 本地方法：生成白圈+红点的录制图标
         auto makeRecordingIcon = [](const QSize& size) -> QIcon {
             QSize s = size.isValid() ? size : QSize(70, 70);
             QPixmap pm(s);
             pm.fill(Qt::transparent);
             QPainter p(&pm);
             p.setRenderHint(QPainter::Antialiasing, true);
             const int w = s.width();
             const int h = s.height();
             const int d = qMin(w, h);
             const QPoint center(w / 2, h / 2);
             const int margin = qMax(2, d * 11 / 100);       // 11%
             const int stroke = qMax(2, d * 13 / 100);       // 13%
             const int outerR = d / 2 - margin;
             QPen pen(QColor(255, 255, 255), stroke);
             pen.setCapStyle(Qt::RoundCap);
             pen.setJoinStyle(Qt::RoundJoin);
             p.setPen(pen);
             p.setBrush(Qt::NoBrush);
             p.drawEllipse(center, outerR, outerR);
             // 内部红点
             p.setPen(Qt::NoPen);
             p.setBrush(QColor(255, 59, 48));                // iOS系统红
             const int innerR = qMax(2, d * 15 / 100);       // 15%
             p.drawEllipse(center, innerR, innerR);
             p.end();
             return QIcon(pm);
         };
         // 初始图标为开始
         recordBtn->setIcon(makeWhiteIcon(":/icons/record_start.svg"));
         recordBtn->setCheckable(true);
         recordBtn->setChecked(false);
         // 不显示文本（只显示图标）
         recordBtn->setText("");
 
         // 点击切换开始/结束
         connect(recordBtn, &QPushButton::clicked, this, [this, recordBtn]() {
             if (!m_screenRecorder) return;
             if (!m_screenRecorder->isRecording()) {
                 m_screenRecorder->startRecording();
             } else {
                 m_screenRecorder->stopRecording();
             }
         });
 
         // 计时更新到底部文本
         connect(m_screenRecorder, &ScreenRecorderOverlay::elapsedUpdated, this, [this](const QString& t){
             if (m_toolBar) {
                 m_toolBar->setBottomInfoText(t);
             }
         });
 
         // 开始/结束时更新图标与底部时间可见性
         connect(m_screenRecorder, &ScreenRecorderOverlay::recordingStarted, this, [this, recordBtn, makeRecordingIcon](const QString& path){
             recordBtn->setIcon(makeRecordingIcon(recordBtn->iconSize()));
             if (m_toolBar) m_toolBar->setBottomInfoVisible(true);
             // 提示开始录制与输出路径
             showToast(this, QString("开始录制: %1").arg(path), 2000);
         });
         connect(m_screenRecorder, &ScreenRecorderOverlay::recordingStopped, this, [this, recordBtn, makeWhiteIcon](const QString& path, bool){
             // 停止后直接恢复为开始图标（白色）
             recordBtn->setIcon(makeWhiteIcon(":/icons/record_start.svg"));
             if (m_toolBar) {
                 m_toolBar->setBottomInfoVisible(false);
                 m_toolBar->setBottomInfoText("");
             }
             // 提示保存成功
             showToast(this, QString("录制已保存: %1").arg(path), 2000);
         });
     }
 
     // 添加部件到主布局
     mainLayout->addWidget(contentContainer, 1);    // 内容区域
    
    // 直接显示导航栏和状态栏并提升层级
    m_navigationBar->show();
    m_navigationBar->raise();
    
    m_statusBar->show();
    m_statusBar->raise();
    
    // 初始更新导航栏和状态栏位置
    updateNavigationBarPosition();
    updateStatusBarPosition();
    updateRecorderPosition();
    
    // 确保所有浮动组件在最上层
    m_toolBar->raise(); // 确保工具栏在最上层
    m_navigationBar->raise(); // 确保导航栏在最上层
    m_statusBar->raise(); // 确保状态栏在最上层
    if (m_screenRecorder) m_screenRecorder->raise();
    
    // 加载样式表
    loadStyleSheet();
    
    // 创建默认目录结构
    createDefaultDirectories();

    // 连接主页的相机模式变更信号到导航栏
    HomePage* homePage = m_pageManager->getHomePage();
    if (homePage && m_navigationBar) {
        connect(homePage, &HomePage::cameraModeChanged,
                m_navigationBar, &NavigationBar::updateMeasurementButtonVisibility);
        LOG_INFO("已连接主页相机模式变更信号到导航栏");
    } else {
        LOG_WARNING("无法连接相机模式变更信号：主页或导航栏为空");
    }

    // 连接导航栏大小改变信号到位置更新
    if (m_navigationBar) {
        connect(m_navigationBar, &NavigationBar::sizeChanged,
                this, &MainWindow::updateNavigationBarPosition);
        LOG_INFO("已连接导航栏大小改变信号到位置更新");
    }

    LOG_INFO("主窗口UI设置完成");
}

// 新增方法：更新导航栏位置
void MainWindow::updateNavigationBarPosition()
{
    if (m_navigationBar) {
        // 计算导航栏位置（底部居中）
        int navBarWidth = m_navigationBar->width();
        int navBarHeight = m_navigationBar->height();
        int navBarX = (width() - navBarWidth) / 2;
        int navBarY = height() - navBarHeight - 10; // 底部留出10像素间距
        
        // 设置导航栏位置
        m_navigationBar->setGeometry(navBarX, navBarY, navBarWidth, navBarHeight);
        LOG_INFO(QString("更新导航栏位置: (%1, %2)").arg(navBarX).arg(navBarY));
    }
}

// 新增方法：更新状态栏位置
void MainWindow::updateStatusBarPosition()
{
    if (m_statusBar) {
        // 状态栏占满屏幕宽度，放置在顶部
        int statusBarHeight = m_statusBar->height();
        
        // 设置状态栏位置（顶部，宽度100%）
        m_statusBar->setGeometry(0, 0, width(), statusBarHeight);
        LOG_INFO(QString("更新状态栏位置: (0, 0)，宽度: %1，高度: %2").arg(width()).arg(statusBarHeight));
    }
}

void MainWindow::updateRecorderPosition()
{
    if (m_screenRecorder) {
        m_screenRecorder->updatePosition(width(), height());
    }
}

void MainWindow::loadStyleSheet()
{
    QFile file(":/styles/dark_theme.qss");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(file.readAll());
        setStyleSheet(styleSheet);
        file.close();
        LOG_INFO("样式表加载成功");
    } else {
        LOG_WARNING(QString("无法加载样式表: %1").arg(file.errorString()));
    }
}

void MainWindow::createDefaultDirectories()
{
    using SmartScope::Infrastructure::ConfigManager;
    QString rootDirectory = ConfigManager::instance().getValue("app/root_directory", QDir::homePath() + "/data").toString();
    // 根目录
    QDir rootDir(rootDirectory);
    if (!rootDir.exists()) rootDir.mkpath(".");

    // Pictures
    QString picturesPath = rootDirectory + "/Pictures";
    QDir picturesDir(picturesPath);
    if (!picturesDir.exists()) picturesDir.mkpath(".");

    // Screenshots
    QString screenshotsPath = rootDirectory + "/Screenshots";
    QDir screenshotsDir(screenshotsPath);
    if (!screenshotsDir.exists()) screenshotsDir.mkpath(".");

    // Videos
    QString recordingsPath = rootDirectory + "/Videos";
    QDir recordingsDir(recordingsPath);
    if (!recordingsDir.exists()) recordingsDir.mkpath(".");

    LOG_INFO(QString("目录已准备: root=%1, pictures=%2, screenshots=%3, videos=%4")
             .arg(rootDirectory).arg(picturesPath).arg(screenshotsPath).arg(recordingsPath));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    LOG_INFO(QString("MainWindow直接捕获到按键事件: 键值=%1 (0x%2), 文本='%3'")
             .arg(event->key())
             .arg(event->key(), 0, 16)
             .arg(event->text()));
    
    // 记录特殊按键
    if (event->key() == Qt::Key_F2) {
        LOG_INFO("检测到F2键按下 - 应进入设置页面");
    } else if (event->key() == Qt::Key_F3) {
        LOG_INFO("检测到F3键按下 - 应进入预览页面");
    } else if (event->key() == Qt::Key_F9) {
        LOG_INFO("检测到F9键按下 - 应触发拍照功能");
    } else if (event->key() == Qt::Key_F12) {
        LOG_INFO("检测到F12键按下 - 应触发LED控制功能");
    }
    
    // 将事件传递给基类处理
    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    
    // 更新工具栏位置
    if (m_toolBar) {
        m_toolBar->updatePosition();
    }
    
    // 更新导航栏位置
    updateNavigationBarPosition();
    
    // 更新状态栏位置
    updateStatusBarPosition();

    // 更新录屏悬浮控件位置
    updateRecorderPosition();
}

// 添加事件过滤器以处理全局事件
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 重要修复：如果事件来自对话框或其子控件，不要拦截任何事件
    QWidget *widget = qobject_cast<QWidget*>(watched);
    if (widget) {
        // 检查是否是对话框或对话框的子控件
        QWidget *topLevel = widget->window();
        if (qobject_cast<QDialog*>(topLevel)) {
            // 这是对话框或对话框的子控件，不拦截任何事件
            if (event->type() == QEvent::MouseButtonPress) {
                qDebug() << "检测到对话框鼠标点击事件，不拦截:" << watched->metaObject()->className();
            }
            return QMainWindow::eventFilter(watched, event);
        }

        // 检查是否是虚拟键盘相关的控件
        QString className = widget->metaObject()->className();
        if (className.contains("VirtualKeyboard", Qt::CaseInsensitive) ||
            className.contains("InputPanel", Qt::CaseInsensitive) ||
            className.contains("Keyboard", Qt::CaseInsensitive)) {
            // 这是虚拟键盘相关控件，不拦截任何事件
            if (event->type() == QEvent::MouseButtonPress) {
                qDebug() << "检测到虚拟键盘鼠标点击事件，不拦截:" << className;
            }
            return QMainWindow::eventFilter(watched, event);
        }
    }

    // 检测键盘事件
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // 重要修复：如果是输入框相关的事件，不要拦截
        // 检查事件来源是否是输入控件
        if (qobject_cast<QLineEdit*>(watched) ||
            qobject_cast<QTextEdit*>(watched) ||
            qobject_cast<QPlainTextEdit*>(watched)) {
            // 对于输入控件，只处理功能键，不处理普通字符输入
            if (keyEvent->key() < Qt::Key_F1 || keyEvent->key() > Qt::Key_F35) {
                // 不是功能键，让输入控件正常处理
                return QMainWindow::eventFilter(watched, event);
            }
        }

        LOG_INFO(QString("全局事件过滤器捕获到按键事件: 对象=%1, 键值=%2 (0x%3)")
                 .arg(watched->metaObject()->className())
                 .arg(keyEvent->key())
                 .arg(keyEvent->key(), 0, 16));
        
        // 获取当前页面类型
        PageType currentPageType = m_pageManager->getCurrentPageType();
        bool isHomePage = (currentPageType == PageType::Home);
        
        // 检查各功能键
        if (keyEvent->key() == Qt::Key_F4) {
            LOG_INFO("全局过滤器检测到F4键 - 应触发AI检测功能切换");
            
            // 获取主页对象
            HomePage* homePage = m_pageManager->getHomePage();
            
            // 如果不在首页，弹出提示框询问是否要离开当前页面
            if (!isHomePage) {
                using namespace SmartScope::App::Ui;
                
                QMessageBox::StandardButton result = DialogUtils::showStyledConfirmationDialog(
                    this,
                    "离开当前页面",
                    "AI检测功能需要返回首页才能操作，是否离开当前页面？",
                    "确定",
                    "取消"
                );
                
                if (result == QMessageBox::Yes) {
                    // 用户确认，切换到主页
                    m_pageManager->switchToPage(PageType::Home);
                    
                    // 短暂延迟后执行AI检测切换
                    QTimer::singleShot(300, [homePage]() {
                        if (homePage) {
                            // 获取当前状态并切换
                            homePage->toggleObjectDetection(!homePage->property("objectDetectionEnabled").toBool());
                        }
                    });
                }
            } else {
                // 已在首页，直接切换AI检测状态
                if (homePage) {
                    // 获取当前状态并切换
                    homePage->toggleObjectDetection(!homePage->property("objectDetectionEnabled").toBool());
                }
            }
            
            return true; // 事件已处理
            
        } else if (keyEvent->key() == Qt::Key_F5) {
            LOG_INFO("全局过滤器检测到F5键 - 应触发3D测量功能");
            
            // 如果不在首页，弹出提示框询问是否要离开当前页面
            if (!isHomePage) {
                using namespace SmartScope::App::Ui;
                
                QMessageBox::StandardButton result = DialogUtils::showStyledConfirmationDialog(
                    this,
                    "离开当前页面",
                    "3D测量功能需要返回首页才能操作，是否离开当前页面？",
                    "确定",
                    "取消"
                );
                
                if (result == QMessageBox::Yes) {
                    // 用户确认，切换到主页
                    m_pageManager->switchToPage(PageType::Home);
                    
                    // 短暂延迟后执行3D测量页面切换
                    QTimer::singleShot(300, [this]() {
                        m_pageManager->switchToPage(PageType::Measurement);
                    });
                }
            } else {
                // 已在首页，直接切换到3D测量页面
                m_pageManager->switchToPage(PageType::Measurement);
            }
            
            return true; // 事件已处理
            
        } else if (keyEvent->key() == Qt::Key_F7) {
            LOG_INFO("全局过滤器检测到F7键 - 应触发返回功能");
            
            // 获取当前页面类型
            PageType currentPageType = m_pageManager->getCurrentPageType();
            bool isHomePage = (currentPageType == PageType::Home);
            
            // 如果当前不在首页，返回上一级页面（通常是返回首页）
            if (!isHomePage) {
                // 显示提示信息
                showToast(this, "正在返回...", 1500);
                
                // 返回首页
                m_pageManager->switchToPage(PageType::Home);
            }
            
            return true; // 事件已处理
            
        } else if (keyEvent->key() == Qt::Key_F8) {
            LOG_INFO("全局过滤器检测到F8键 - 应触发回到首页功能");
            
            // 获取当前页面类型
            PageType currentPageType = m_pageManager->getCurrentPageType();
            bool isHomePage = (currentPageType == PageType::Home);
            
            // 如果当前不在首页，显示提示并回到首页
            if (!isHomePage) {
                // 显示提示信息
                showToast(this, "正在回到首页...", 1500);
                
                // 回到首页
                m_pageManager->switchToPage(PageType::Home);
            }
            
            return true; // 事件已处理
            
        } else if (keyEvent->key() == Qt::Key_F9) {
            LOG_INFO("全局过滤器检测到F9键 - 应触发拍照功能");
            
            // 检查当前页面类型
            PageType currentPage = m_pageManager->getCurrentPageType();
            if (currentPage != PageType::Home) {
                LOG_INFO("当前不在主页，F9拍照功能不可用");
                // 显示提示信息
                if (QWidget* currentWidget = m_pageManager->currentWidget()) {
                    showToast(currentWidget, "拍照功能仅在主页可用", 2000);
                }
                return true; // 事件已处理
            }
            
            // 查找主页对象并调用拍照方法
            HomePage* homePage = m_pageManager->getHomePage();
            if (homePage) {
                LOG_INFO("找到主页对象，调用拍照方法");
                QMetaObject::invokeMethod(homePage, "captureAndSaveImages", Qt::QueuedConnection);
                return true; // 事件已处理
            } else {
                LOG_WARNING("无法找到主页对象");
                if (QWidget* currentWidget = m_pageManager->currentWidget()) {
                    showToast(currentWidget, "拍照功能暂时不可用", 2000);
                }
                return true;
            }
            
        } else if (keyEvent->key() == Qt::Key_F2) {
            LOG_INFO("全局过滤器检测到F2键 - 应进入设置页面");

            // 显示提示信息
            showToast(this, "正在进入设置页面...", 1500);

            // 切换到设置页面
            m_pageManager->switchToPage(PageType::Settings);

            return true; // 事件已处理

        } else if (keyEvent->key() == Qt::Key_F3) {
            LOG_INFO("全局过滤器检测到F3键 - 应进入预览页面");

            // 显示提示信息
            showToast(this, "正在进入预览页面...", 1500);

            // 切换到预览选择页面
            m_pageManager->switchToPage(PageType::Preview);

            return true; // 事件已处理

        } else if (keyEvent->key() == Qt::Key_F12) {
            LOG_INFO("全局过滤器检测到F12键 - 应触发LED控制功能");

            // 在这里可以添加LED控制逻辑
            // ...

            // 暂时不拦截，让其继续传递
        }
    }
    
    // 其他事件交给基类处理
    return QMainWindow::eventFilter(watched, event);
}

// 重写焦点事件处理函数，增加调试输出
void MainWindow::focusInEvent(QFocusEvent *event)
{
    LOG_INFO(QString("MainWindow获得焦点，原因=%1").arg(event->reason()));
    QMainWindow::focusInEvent(event);
}

void MainWindow::focusOutEvent(QFocusEvent *event)
{
    LOG_INFO(QString("MainWindow失去焦点，原因=%1").arg(event->reason()));
    QMainWindow::focusOutEvent(event);
}

// 添加closeEvent函数，在窗口关闭前优雅地释放资源
void MainWindow::closeEvent(QCloseEvent *event)
{
    LOG_INFO("接收到关闭事件，准备释放资源...");

    // 检查是否已经通过导航栏确认过退出，避免重复询问
    bool exitConfirmed = property("exitConfirmed").toBool();

    if (!exitConfirmed) {
        // 使用统一对话框样式询问用户是否确认退出
        using namespace SmartScope::App::Ui;
        QMessageBox::StandardButton result = DialogUtils::showStyledConfirmationDialog(
            this,
            "确认退出",
            "确定要退出程序吗？",
            "确定",
            "取消"
        );

        if (result == QMessageBox::No) {
            LOG_INFO("用户取消退出");
            event->ignore();
            return;
        }
    } else {
        LOG_INFO("已通过导航栏确认退出，跳过重复询问");
    }
    
    LOG_INFO("用户确认退出，开始释放资源...");

    // 立即隐藏窗口，避免闪烁
    hide();

    // 先关闭LED控制器
    if (LedController::instance().isConnected()) {
        LOG_INFO("关闭LED控制器...");
        // 将灯光亮度设置为0
        LedController::instance().setLightLevel(4); // 最后一个级别是关闭
        LedController::instance().shutdown();
    }

    // 关闭统一设备控制器
    if (DeviceController::instance().isConnected()) {
        LOG_INFO("关闭统一设备控制器...");
        DeviceController::instance().shutdown();
    }

    // 停止推理服务
    LOG_INFO("关闭推理服务...");
    SmartScope::InferenceService::instance().shutdown();

    // 清理可能的临时文件
    LOG_INFO("清理临时文件...");

    // 移除不必要的页面切换，避免UI闪烁
    // 移除事件处理，避免触发不必要的UI更新
    // 移除Toast提示，避免在退出时显示额外UI

    // 接受关闭事件
    LOG_INFO("所有资源已释放，允许程序退出");
    event->accept();
} 